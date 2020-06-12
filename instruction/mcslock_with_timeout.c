typedef struct mcs_qnode {
  volatile struct mcs_qnode *volatile prev;
  volatile struct mcs_qnode *volatile next;
} mcs_qnode;
typedef volatile mcs_qnode *mcs_qnode_ptr;
typedef mcs_qnode_ptr mcs_try_lock;

/* We assume that all valid qnode pointers are word-aligned addresses
    (so the two low-order bits are zero) and are larger than 0x10.  We
    use low-order bits 01 to indicate restoration of a prev pointer that
    had temporarily been changed.  We also use it to indicate that a next
    pointer has been restored to nil, but the process that restored it
    has not yet fixed the global tail pointer.  We use low-order bits of
    10 to indicate that a lock has been granted (If it is granted to a
    process that had been planning to leave, the process needs to know
    who its [final] predecessor was, so it can synch up and allow that
    predecessor to return from release).  Those two bits are mutually
    exclusive, though we don't take advantage of the fact.  We use bogus
    pointer values with low-order 0 bits to indicate that a process or
    its neighbor intends to leave, or has recently done so. */

#define nil             ((qnode_ptr) 0x0)
#define leaving_self    ((qnode_ptr) 0x4)
#define leaving_other   ((qnode_ptr) 0x8)
#define gone            ((qnode_ptr) 0xc)

#define restored_tag    ((unsigned long) 0x1)
#define transient_tag   restored_tag
#define granted_tag     ((unsigned long) 0x2)
#define is_restored(p)  ((unsigned long) (p) & restored_tag)
#define is_transient(p) is_restored(p)
#define is_granted(p)   ((unsigned long) (p) & granted_tag)
#define restored(p)     ((qnode_ptr)((unsigned long) (p) | restored_tag))
#define transient(p)    restored(p)
#define granted(p)      ((qnode_ptr)((unsigned long) (p) | granted_tag))
#define cleaned(p)      ((qnode_ptr)(((unsigned long) (p)) & ~0x3))
#define is_real(p)      (!is_granted(p) && ((unsigned long) (p) > 0x10))

/* prev pointer has changed from pred to temp, and we know temp is
    a real (though possibly restored) pointer.  If temp represents a
    new predecessor, update pred to point to it, clear restored flag, if
    any on prev pointer, and set next field of new predecessor (if not
    restored) to point to me.  In effect, RELINK is step 6.
*/
#define RELINK                                                      \
        do {                                                        \
            if (is_restored(temp)) {                                \
                (void) compare_and_swap(&I->prev,                   \
                        temp, cleaned(temp));                       \
                    /* remove tag bit from pointer, if it hasn't    \
                        been changed to something else */           \
                temp = cleaned(temp);                               \
            } else {                                                \
                pred = temp;                                        \
                temp = swap(&pred->next, I);                        \
                if (temp == leaving_self) {                         \
                    /* Wait for precedessor to notice I've set its  \
                        next pointer and move on to step 2          \
                        (or, if I'm slow, step 5). */               \
                    do {                                            \
                        temp = I->prev;                             \
                    } while (temp == pred);                         \
                    continue;                                       \
                }                                                   \
            }                                                       \
            break;                                                  \
        } while (is_real(temp));

bool mcs_try_acquire(mcs_try_lock *L, qnode_ptr I, hrtime_t T) {
  qnode_ptr pred;
  qnode_ptr succ;
  qnode_ptr temp;
  hrtime_t start = gethrtime();       // high resolution clock

  I->next = nil;
  pred = swap(L, I);
  if (pred == nil) {                  // lock was free
    return true;
  }
  // else queue was non-empty
  I->prev = transient(nil);           // word on which to spin
  // Make predecessor point to me, but preserve transient tag if set.
  do {
    temp = pred->next;
  } while (!compare_and_store(&pred->next, temp,
                              (is_transient(temp) ? transient(I) : I)));
  /* Old value (temp) can't be granted, because predecessor sets that
      only via CAS from a legitimate pointer value.  Might be nil
      (possibly transient) or leaving_self, though. */
  if (is_transient(temp)) {
    while (I->prev == transient(nil));  // spin
    /* Wait for process that used to occupy my slot in the queue
        to clear the transient tag on our (common) predecessor's
        next pointer.  The predecessor needs this cleared so it
        knows when it's safe to return and deallocate its qnode.
        I need to know in case I ever time out and try to leave:
        the previous setting has to clear before I can safely
        set it again. */
    /* Store pred for future reference, if predecessor has not
        modified my prev pointer already. */
    (void) compare_and_swap(&I->prev, nil, pred);
  } else if (temp == leaving_self) {
    /* I've probably got the wrong predecessor.
        Must wait to learn id of real one, *without timing out*. */
    do {
      temp = I->prev;
    } while (temp == transient(nil));
    if (is_real(temp)) {
      RELINK
    }
  } else {
    /* Store pred for future reference, if predecessor has not
        modified my prev pointer already. */
    (void) compare_and_swap(&I->prev, transient(nil), pred);
  }

  while (1) {
    do {
      if (gethrtime() - start > T) {
        goto timeout;       // 2-level break
      }
      temp = I->prev;
    } while (temp == pred);     // spin
    if (is_granted(temp)) {     // this test is an optimization
      break;
    }
    if (temp == leaving_other) {
      // Predecessor is leaving;
      // wait for identity of new predecessor.
      do {
        temp = I->prev;
      } while (temp == leaving_other);
    }
    if (is_granted(temp)) {
      break;
    }
    RELINK
  }
  // Handshake with predecessor.
  pred = cleaned(temp);
  pred->next = nil;
  return true;

timeout:    // Timeout has expired; try to leave.

  /* Step 1: Atomically identify successor (if any)
      and indicate intent to leave. */
  while (1) {
    do {
      do {
        succ = I->next;
      } while (is_transient(succ));
    } while (!compare_and_store(&I->next, succ, leaving_self));
    // don't replace a transient value
    if (succ == gone) {
      succ = nil;
    }
    if (succ == nil) {
      /* No visible successor; we'll end up skipping steps 2 and
          doing an alternative step 6. */
      break;
    }
    if (succ == leaving_other) {
      // Wait for new successor to complete step 6.
      do {
        temp = I->next;
      } while (temp == leaving_self);
      continue;
    }
    break;
  }

  /* Step 2: Tell successor I'm leaving.
      This takes precedence over successor's desire to leave. */
  if (succ != nil) {
    temp = swap(&succ->prev, leaving_other);
    if (temp == leaving_self) {
      /* Successor is also trying to leave, and beat me to the
          punch.  Must wait until it tries to modify my next
          pointer, at which point it will know I'm leaving, and
          will wait for me to finish. */
      do {
        temp = I->next;
      } while (temp != succ);
      /* I'm waiting here for my successor to fail step 4.
          Once it realizes I got here first, it will change my
          next pointer to point back to its own qnode. */
    }
  }

  /* Step 3: Atomically identify predecessor
      and indicate intent to leave. */
  while (1) {
    qnode_ptr n;
    temp = swap(&I->prev, leaving_self);
    if (is_granted(temp)) {
      goto serendipity;
    }
    if (temp == leaving_other) {
      // Predecessor is also trying to leave; it takes precedence.
      do {
        temp = I->prev;
      } while (temp == leaving_self || temp == leaving_other);
      if (is_granted(temp)) {
        /* I must have been asleep for a while.  Predecessor won
            the race, then experienced serendipity itself, restored
            my prev pointer, finished its critical section, and
            released. */
        goto serendipity;
      }
      RELINK
      continue;
    }
    if (temp != pred) {
      RELINK
      /* I'm about the change pred->next to leaving_other or
          transient(nil), so why set it to I first?  Because other-
          wise there is a race.  I need to avoid the possibility
          that my (new) predecessor will swap leaving_self into its
          next field, see leaving_other (left over from its old
          successor), and assume I won the race, after which I come
          along, swap leaving_other in, get leaving_self back, and
          assume my predecessor won the race. */
    }

    // Step 4: Tell predecessor I'm leaving.
    if (succ == nil) {
      n = swap(&pred->next, transient(nil));
    } else {
      n = swap(&pred->next, leaving_other);
    }
    if (is_granted(n)) {
      // Predecessor is passing me the lock; wait for it to do so.
      do {
        temp = I->prev;
      } while (temp == leaving_self);
      goto serendipity;
    }
    if (n == leaving_self) {
      /* Predecessor is also trying to leave.  I got to step 3
          before it got to step 2, but it got to step 1 before
          I got to step 4.  It will wait in Step 2 for me to
          acknowledge that my step 4 has failed. */
      pred->next = I;
      // Wait for new predecessor
      // (completion of old predecessor's step 5)
      do {
        temp = I->prev;
      } while (temp == leaving_self /* what I set */
               || temp == leaving_other /* what pred will set in step 2 */ );
      if (is_granted(temp)) {
        goto serendipity;
      }
      RELINK
      continue;
    }
    break;
  }

  if (succ == nil) {
    // Step 5: Try to fix global pointer.
    if (compare_and_store(L, I, pred)) {
      // Step 6: Try to clear transient tag.
      temp = compare_and_swap(&pred->next, transient(nil), gone);
      if (temp != transient(nil)) {
        pred->next = cleaned(temp);
        /* New successor got into the timing window, and is now
            waiting for me to signal it. */
        temp = cleaned(temp);
        (void) compare_and_swap(&temp->prev, transient(nil), nil);
      }
    } else {
      /* Somebody has gotten into line.  It will discover that
          I'm leaving and wait for me to tell it who the real
          predecessor is (see pre-timeout loop above). */
      do {
        succ = I->next;
      } while (succ == leaving_self);
      pred->next = leaving_other;
      succ->prev = pred;
    }
  } else {
    // Step 5: Tell successor its new predecessor.
    succ->prev = pred;
  }
  // Step 6: Count on successor to introduce itself to predecessor.

  return false;

serendipity:    // I got the lock after all;
  // temp contains a granted value read from my prev pointer.

  // Handshake with predecessor:
  pred = cleaned(temp);
  pred->next = nil;
  if (succ == nil) {
    // I don't think I have a successor.  Try to undo step 1.
    if ((temp = compare_and_swap(&I->next, leaving_self, succ))
        != leaving_self) {
      /* I have a successor after all.  It will be waiting for me
          to tell it who its real predecessor is.  Since it's me
          after all, I have to tag the value so successor will
          notice change. */
      // Undo step 2:
      temp->prev = restored(I);
    }
  } else {
    /* I have a successor, which may or may not have been trying
        to leave.  In either case, I->next is now correct. */
    // Undo step 1:
    I->next = succ;
    // Undo step 2:
    succ->prev = restored(I);
  }
  return true;
}

void mcs_release(mcs_try_lock *L, qnode_ptr I) {
  qnode_ptr succ;
  qnode_ptr temp;

  while (1) {
    succ = I->next;
    if (succ == leaving_other || is_transient(succ)) {
      // successor is leaving, but will update me.
      continue;
    }
    if (succ == gone) {
      if (compare_and_store(&I->next, gone, nil)) {
        succ = nil;
      } else {
        continue;
      }
    }
    if (succ == nil) {
      // Try to fix global pointer.
      if (compare_and_store(L, I, nil)) {
        /* Make sure anybody who happened to sneak in and leave
            again has finished looking at my qnode. */
        do {
          temp = I->next;
        } while (is_transient(temp));
        return;     // I was last in line.
      }
      do {
        temp = I->next;
      } while (temp == nil);
      continue;
    }
    if (compare_and_store(&I->next, succ, granted(nil))) {
      /* I have atomically identified my successor and
          indicated that I'm releasing; now tell successor it
          has the lock. */
      temp = swap(&succ->prev, granted(I));
      /* Handshake with successor to make sure it won't try to
          access my qnode any more (might have been leaving). */
      do {
        temp = I->next;
      } while (temp != nil);
      return;
    }
    /* Else successor changed.  Continue loop.  Note that every
        iteration sees a different (real) successor, so there isn't
        really a global spin */
  }
}

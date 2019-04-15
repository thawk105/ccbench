# Redesign and Implement Many Concurrency Control

---

## Snapshot Isolation

## SS2PL
Strong Strict 2 Phase Lock.  
### Implemented
- Lock  
  - reader/writer lock  

- Deadlock resolution  
  - No-Wait

---

## Silo
It was proposed at SOSP'2013 by Stephen Tu.
- modify
  - reduce roop in concurrency control protocol
---

## ERMIA (SI + SSN)
ERMIA was proposed at SIGMOD'2016 by Kangnyeon Kim.  
SSN is serialization certifier which has to be executed serial.  
Latch-free SSN was proposed at VLDB'2017 by Tianzheng Wang.  

### Implemented
- original
  - SSN  
  - Latch-free SSN  
    - Leveraging existing infrastructure
- modify
  - Garbage Collection for Transaction Mapping Table

---

## TicToc
TicToc was proposed at SIGMOD'2016 by Xiangyao Yu.  

### Implemented
- original
  - all opt.
    - No-Wait Locking in Validation Phase
    - Preemptive Aborts
    - Timestamp History
- modify
  - reduce roop in concurrency control protocol

---

## MOCC
MOCC was proposed at VLDB'2017 by Tianzheng Wang.  

### Implemented
- modify
  - new temprature protocol reduces contentions and improves throughput much.

---

## Cicada
Cicada was proposed at SIGMOD'2017 by Hyeontaek Lim.  

### Implemented
- almost all opt
  - Sort write set by contention
  - Pre-check version consistency
  - Eary aborts
- modify
  - modify consistency checks because original protocl breaks consistency.

---

## Instruction Survey
* fetch\_add
* xoroshiro128+
* memory benchmark

---
This activity was supported by Cybozu Labs Youth 8th term. (2018/4/10 - 2019/4/10)

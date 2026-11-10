# CSE/PC/B/S/314 Computer Networks Lab

**Course Outcome 3 (CO3):** Design and implement medium access control mechanisms within a simulated network environment using IEEE 802 standards.  
**Assignment 3:** Implement CSMA techniques with collision detection.  
**Code demonstration due on:** 21-25 September 2026 (in your respective lab classes)  
**Report submission due on:** Next lab classes  
**Softcopy of the report:** Upload to the drive.  
**Language:** High-level language like C, C++, Java, Python, etc.

---

## Problem Statement

> Please note that you may need to use these schemes separately for other applications (assignments).  
> Use the same sender-receiver design as previous assignments.

In this assignment, you have to emulate the shared channel (maintaining its state as `IDLE`, `BUSY`, or `COLLISION`), while multiple independent client processes emulate network stations that sense the channel and attempt transmission according to a given CSMA strategy:
1. **Non-Persistent CSMA**
2. **1-Persistent CSMA**
3. **p-Persistent CSMA**
4. **CSMA/CD (Carrier Sense Multiple Access with Collision Detection)**

Write methods/functions to emulate:
* Carrier sensing
* Collision detection
* Backoff behavior

Analyze how the number of contending stations affects:
* The number of collisions
* Average transmission delay
* Overall channel throughput

---

## Experimental Requirements

Test the above schemes for the following cases (not limited to):

1. **Case (i) — For p-persistent CSMA:**
   * Plot number of collisions, transmission delay, and throughput as functions of $p$ (fixed $N$).

2. **Case (ii) — For all schemes (Non-Persistent, 1-Persistent, p-Persistent at best $p$, CSMA/CD):**
   * Plot the same three metrics (collisions, transmission delay, throughput) as functions of the number of contending stations $N$.
   * Compare their efficiency.

3. **Observations:**
   * State your observations on the impact of performance of CSMA techniques in different scenarios.

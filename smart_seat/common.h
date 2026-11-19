// common.h - shared enums and global variables (declarations only).
// The actual variables are defined ONCE in smart_seat.ino.
// Rule: only the owner listed next to a variable may write it; everyone else reads.
#ifndef COMMON_H
#define COMMON_H

enum PostureState { POSTURE_GOOD, POSTURE_BAD_NECK, POSTURE_BAD_LEAN, POSTURE_UNKNOWN };
enum SeatState    { SEAT_AVAILABLE, SEAT_OCCUPIED, SEAT_AWAY };   // blue / red / yellow
enum UseMode      { MODE_LAPTOP, MODE_BOOK };

extern PostureState  postureState;          // main (1 + 4 combined)
extern SeatState     seatState;             // main, updateSeatState() only
extern UseMode       useMode;               // 7
extern bool          isPersonPresent;       // 4
extern bool          isAway;                // 8
extern float         neckAngleDeg;          // 1
extern bool          neckBad;               // 1
extern bool          leanBad;               // 4
extern float         loadLeftG;             // 4
extern float         loadRightG;            // 4
extern float         ultrasonicDistanceCm;  // 2
extern bool          irDetected;            // 3
extern unsigned long focusTimeSec;          // 5
extern unsigned long awayRemainSec;         // 5
extern unsigned long seatRemainSec;         // 5

#endif

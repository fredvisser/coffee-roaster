#ifndef TYPES_HPP
#define TYPES_HPP

// Roaster state machine states
enum RoasterState
{
  IDLE = 0,
  START_ROAST = 1,
  ROASTING = 2,
  COOLING = 3,
  ERROR = 4
};

#endif // TYPES_HPP

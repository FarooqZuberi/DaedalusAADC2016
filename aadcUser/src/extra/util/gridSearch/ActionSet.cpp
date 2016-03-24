#include "ActionSet.h"
#include <iostream>
 
// -------------------------------------------------------------------------------------------------
ActionSet::ActionSet() {
// -------------------------------------------------------------------------------------------------
  /*Action action_set[] = {
    Action(-4, -5, 60, 1, -30, 26), // -30 degree
    Action(-3, -5, 40, 1, -20, 27), // -20 degree
    Action(-2, -6, 20, 1, -10, 28), // -10 degree
    Action(0, -7, 0, 1, -5, 28), // 0 degree
    Action(2, -6, 10, 1, 10, 28), // 10 degree
    Action(3, -5, 30, 1, 20, 27), // 20 degree
    Action(4, -5, 50, 1, 30, 26) // 30 degree
  };*/
  
  /*Action action_set[] = {
    Action(-6, -5, 80, 1, -30, 26), // -30 degree
    Action(-4, -5, 60, 1, -30, 26), // -30 degree
    Action(-3, -5, 40, 1, -20, 27), // -20 degree
    Action(-2, -6, 20, 1, -13, 28), // -10 degree
    Action(0, -7, 0, 1, -5, 28), // 0 degree
    Action(2, -6, 10, 1, 10, 28), // 10 degree
    Action(3, -5, 30, 1, 18, 27), // 20 degree
    Action(4, -5, 50, 1, 25, 26), // 30 degree
    Action(6, -5, 70, 1, 26, 26) // 30 degree
  };*/
  
  Action action_set[] = {
    Action(-6, -5, 80, 1, -30, 24), // -30 degree
    Action(-4, -5, 60, 1, -30, 24), // -30 degree
    Action(-3, -5, 40, 1, -20, 24), // -20 degree
    Action(-2, -6, 20, 1, -13, 25), // -10 degree
    Action(0, -7, 0, 1, -5, 27), // 0 degree
    Action(2, -6, 10, 1, 10, 25), // 10 degree
    Action(3, -5, 30, 1, 18, 24), // 20 degree
    Action(4, -5, 50, 1, 25, 24), // 30 degree
    Action(6, -5, 70, 1, 26, 24) // 30 degree
  };
  
  size_ = sizeof(action_set)/sizeof(Action);
  action_set_ = new Action[size_];
  for (int i = 0; i < size(); i++) action_set_[i] = action_set[i];
}

// -------------------------------------------------------------------------------------------------
ActionSet::~ActionSet() {
// -------------------------------------------------------------------------------------------------
  delete[] action_set_;
}

// -------------------------------------------------------------------------------------------------
Action ActionSet::operator[](int index) {
// -------------------------------------------------------------------------------------------------
  return action_set_[index];
}

// -------------------------------------------------------------------------------------------------
int ActionSet::size() {
// -------------------------------------------------------------------------------------------------
  return size_;
}
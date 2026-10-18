#ifndef CODECRAFTERS_SQLITE_EXECUTE_H
#define CODECRAFTERS_SQLITE_EXECUTE_H

#include "database.h"
#include "plan.h"

// runs a planned SELECT against db, printing its result to stdout: one line
// per matching row with | between values, or the count for COUNT. Prints its
// own error message to stderr and returns nonzero on failure.
int execute_select(const Database* db, const SelectPlan* plan);

#endif // CODECRAFTERS_SQLITE_EXECUTE_H

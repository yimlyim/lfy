// Defines a LogRepository class that manages loggers and their configurations.
// a logger is identified by a path, which is a string. Dot separated paths are used to identify
// nested loggers. The repository uses each path item to allow for hierarchical loggers, which
// inherit properties from their parent loggers, such as outputters and log levels. The levels are
// overwriteable, meaning that a child logger can have a different log level than its parent.
#pragma once
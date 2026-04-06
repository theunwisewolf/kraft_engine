namespace kraft {

void LogAssert(const char* expression, const char* message, const char* file, int line) {
    KFATAL("Assertion failure! Expression: '%s' | Message: '%s' | File: %s | Line: %d", expression, message, file, line);
    fflush(stdout);
    fflush(stderr);
}

} // namespace kraft
namespace kraft {

void LogAssert(const char* expression, const char* message, const char* file, int line) {
    char text[1024];
    snprintf(text, sizeof(text), "Assertion failure! Expression: '%s' | Message: '%s' | File: %s | Line: %d\n", expression, message, file, line);
    LogHistoryPushLine(text);
    fputs(text, stdout);
    fputs(text, stderr);
    fflush(stdout);
    fflush(stderr);
}

} // namespace kraft
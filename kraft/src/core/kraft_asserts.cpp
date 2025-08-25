namespace kraft {

void LogAssert(const char* Expression, const char* Message, const char* File, int Line)
{
    KFATAL("Assertion failure! Expression: '%s' | Message: '%s' | File: %s | Line: %d", Expression, Message, File, Line);
}

}
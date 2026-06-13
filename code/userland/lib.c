#include "syscall.h"
#define NULL  ((void *) 0)

unsigned
strlen(const char *s)
{
    if (s == NULL) {
        return 0;
    }

    unsigned i;
    for (i = 0; s[i] != '\0'; i++) {}
    return i;
}

int
PrintString(const char *s)
{
    if (s == NULL) {
        return 0;
    }

    unsigned len = strlen(s);
    return Write(s, len, CONSOLE_OUTPUT);
}

int
PrintChar(char c)
{
    return Write(&c, 1, CONSOLE_OUTPUT);
}

void
myPuts(const char *s)
{
    if (s == NULL) {
        return;
    }

    PrintString(s);
    PrintChar('\n');
}

static void
reverse(char *str)
{
    if (str == NULL) {
        return;
    }

    unsigned left = 0;
    unsigned right = strlen(str) - 1;
    while (left < right) {
        char temp = str[left];
        str[left] = str[right];
        str[right] = temp;
        left++;
        right--;
    }
}

void
itoa(int num, char *str)
{
    if (str == NULL) {
        return;
    }

    unsigned i = 0;
    int isNegative = 0;
    
    if (num < 0) {
        isNegative = 1;
        num = -num;
    }

    if (num == 0) {
        str[0] = '0';
        str[1] = '\0';
        return;
    }

    for (; num > 0; i++) {
        str[i] = num % 10 + '0';
        num /= 10;
    }
    str[i] = '\0';
    
    reverse(str);
    
    if (isNegative) {
        for (unsigned j = i; j > 0; j--) {
            str[j] = str[j - 1];
        }
        str[0] = '-';
    }
}

unsigned
ReadLine(char *buffer, unsigned size)
{
    if (buffer == NULL || size == 0) {
        return 0;
    }

    unsigned i;
    for (i = 0; i < size - 1; i++) {
        Read(&buffer[i], 1, CONSOLE_INPUT);
        if (buffer[i] == '\n') {
            buffer[i] = '\0';
            break;
        }
    }
    return i;
}
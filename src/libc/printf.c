#include <stdio.h>
#include <string.h>
#include <symbol_attr.h>

//Use hidden internal function from libultra
extern int _Printf(char *(*copyfunc)(char *, char *, size_t), void*, const char*, va_list);

static char *WriteBuf(char *dst, char *src, size_t len)
{
    return (char *)memcpy(dst, src, len) + len;
}

//vsnprintf buffer parameters
static char *vsnprintf_buf;
static size_t vsnprintf_len;

static char *WriteVsnprintf(char *dst, char *src, size_t len)
{
	if(vsnprintf_len == 0) {
		//Special Case for 0-length buffer
		return src+len;
	} else {
		char *dst_end = vsnprintf_buf+vsnprintf_len;
		if(dst > dst_end) {
			//Out of buffer space
			return dst+len;
		} else {
			if(dst+len > dst_end) {
				//Will be out of buffer space
				return (char *)memcpy(dst, src, dst_end-dst) + len;
			} else {
				//Still have buffer space
				return (char *)memcpy(dst, src, len) + len;
			}
		}
	}
}

EXPORT_SYMBOL int vsnprintf(char *str, size_t n, const char *format, va_list arg)
{
	int written;
	vsnprintf_buf = str;
	vsnprintf_len = n;
    written = _Printf(WriteVsnprintf, str, format, arg);
    if(n != 0 && written >= 0 && written < n) {
        str[written] = 0;
    }
    return written;
}

EXPORT_SYMBOL int snprintf(char *str, size_t n, const char *format, ...)
{
	va_list arg;
	va_start(arg, format);
	vsnprintf(str, n, format, arg);
	va_end(arg);
}

EXPORT_SYMBOL int vsprintf(char *str, const char *format, va_list arg)
{
	int written;
	written = _Printf(WriteBuf, str, format, arg);
	if (written >= 0) {
		str[written] = 0;
	}
	return written;
}
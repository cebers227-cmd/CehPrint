#include <stdarg.h>
#include <stdio.h>
#include "i18n.h"

const stp_string_list_t *stp_i18n_load(const char *locale)
{
  (void)locale;
  return NULL;
}

const char *stp_i18n_lookup(const stp_string_list_t *po, const char *message)
{
  (void)po;
  return message;
}

void stp_i18n_printf(const stp_string_list_t *po, const char *message, ...)
{
  va_list ap;
  (void)po;
  va_start(ap, message);
  vfprintf(stderr, message, ap);
  va_end(ap);
}

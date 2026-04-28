#include <stdio.h>
#include "eni/session.h"
static eni_session_t s;
int main(void) {
    printf("1\n"); fflush(stdout);
    memset(&s, 0, sizeof(s));
    printf("2\n"); fflush(stdout);
    eni_status_t rc = eni_session_init(&s);
    printf("3 rc=%d\n", rc); fflush(stdout);
    eni_session_destroy(&s);
    printf("done\n");
    return 0;
}

#include "fs.h"

int main(void)
{
    const char *path = "repository-lock.test";
    struct cv_lock first, second, exclusive, blocked;
    if (!cv_lock_file(path, 0, &first)) {
        return 1;
    }
    if (!cv_lock_file(path, 0, &second)) {
        cv_unlock_file(&first);
        return 1;
    }
    int acquired = cv_lock_file(path, 1, &blocked);
    if (acquired) {
        cv_unlock_file(&blocked);
    }
    cv_unlock_file(&second);
    cv_unlock_file(&first);
    if (acquired || !cv_lock_file(path, 1, &exclusive)) {
        return 1;
    }
    acquired = cv_lock_file(path, 0, &blocked);
    if (acquired) {
        cv_unlock_file(&blocked);
    }
    cv_unlock_file(&exclusive);
    return acquired;
}

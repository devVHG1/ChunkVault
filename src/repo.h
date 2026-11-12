#ifndef CV_REPO_H
#define CV_REPO_H

int cv_repo_init(const char *root);
int cv_repo_backup(const char *root, const char *source, const char *snapshot);
int cv_repo_list(const char *root);
int cv_repo_verify(const char *root);
int cv_repo_read(const char *root, const char *snapshot, const char *output);

int cv_repo_recover(const char *root, int remove);

#endif

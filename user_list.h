#ifndef USER_LIST_H
#define USER_LIST_H

struct user;
struct user_list;

void user_list_init(struct user_list *users);
struct user *user_list_find_by_nickname(struct user_list *users, const char *nickname);
struct user *user_list_find_by_socket(struct user_list *users, int socket_fd);
int user_list_add(struct user_list *users, struct user *user);
int user_list_remove(struct user_list *users, int socket_fd);
void user_list_destroy(struct user_list *users);

#endif

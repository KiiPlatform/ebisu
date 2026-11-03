/**
 * @file ebisu_object.h
 * @brief How ebisu calls the objects an application gives it.
 *
 * A socket, the system and a lock are each a struct holding a pointer to a
 * table of operations. ebisu calls an operation with the struct itself as its
 * first argument, its self. An implementation embeds the struct in one of its
 * own, which holds its state, and gets that back from self with
 * EBISU_CONTAINER_OF():
 *
 * \code
 * typedef struct my_socket_t {
 *     ebisu_socket_t base;
 *     int fd;
 * } my_socket_t;
 *
 * static ebisu_sock_code_t my_close(ebisu_socket_t* self)
 * {
 *     my_socket_t* sock = EBISU_CONTAINER_OF(self, my_socket_t, base);
 *     close(sock->fd);
 *     return EBISU_SOCK_OK;
 * }
 * \endcode
 *
 * The table is normally const, shared by every object of a kind, so that on a
 * microcontroller it stays in flash and each object costs one pointer. ebisu
 * keeps a pointer to the object, so it has to outlive whatever it is set on.
 */
#ifndef _EBISU_OBJECT
#define _EBISU_OBJECT

#include <stddef.h>

/**
 * \brief The struct of type type whose member member ptr points to.
 *
 * \param [in] ptr Pointer to the member, such as the self an operation gets.
 * \param [in] type Type of the struct embedding it.
 * \param [in] member Name of the member in that struct.
 */
#define EBISU_CONTAINER_OF(ptr, type, member) \
    ((type*)(void*)((char*)(ptr) - offsetof(type, member)))

#endif /* _EBISU_OBJECT */
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */

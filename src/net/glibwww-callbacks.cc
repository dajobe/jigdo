/* $Id$ -*- C++ -*-

  This code was taken from glibwww2
  <http://cvs.gnome.org/lxr/source/glibwww2/>, main author: James
  Henstdridge <james@daa.com.au>, distributable under GPL, v2 or
  later.

  Added support for compilation under Windows (mingw32).

*/
/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#include <config.h>
#include <glibwww.hh>
#undef PACKAGE
#undef _
extern "C" {
#include <HTEvent.h>
#include <HTTimer.h>
}

#define WWW_HIGH_PRIORITY (G_PRIORITY_HIGH_IDLE + 50)
#define WWW_LOW_PRIORITY G_PRIORITY_LOW
#define WWW_SCALE_PRIORITY(p) ((WWW_HIGH_PRIORITY - WWW_LOW_PRIORITY) * p \
                                    / HT_PRIORITY_MAX + WWW_LOW_PRIORITY)

#define READ_CONDITION (G_IO_IN | G_IO_HUP | G_IO_ERR)
#define WRITE_CONDITION (G_IO_OUT | G_IO_ERR)
#define EXCEPTION_CONDITION (G_IO_PRI)
/* Windows: Need G_IO_IN for WRITE_CONDITION, otherwise connect() hangs */
#ifdef G_OS_WIN32
#       undef WRITE_CONDITION
#       define WRITE_CONDITION (G_IO_IN | G_IO_OUT | G_IO_ERR)
#endif

#if HTEVENT_TYPES != 3
#       warning "HTEVENT_TYPES != 3 not supported"
#       warning "Compile libwww without WWW_WIN_ASYNC for Windows!"
#endif

typedef struct _SockEventInfo SockEventInfo;
struct _SockEventInfo {
        SOCKET s;
        HTEventType type;
        HTEvent *event;
        guint io_tag;
        guint timer_tag;
};

typedef struct _SockInfo SockInfo;
struct _SockInfo {
        SOCKET s;
        GIOChannel *io;
        SockEventInfo ev[HTEvent_TYPES];
};

static GHashTable *sockhash = NULL;


static SockInfo *
get_sock_info(SOCKET s, gboolean create)
{
        SockInfo *info;

        if (!sockhash)
                sockhash = g_hash_table_new(g_direct_hash, g_direct_equal);

        info = static_cast<SockInfo*>(
            g_hash_table_lookup(sockhash, GINT_TO_POINTER(s)));
        if (!info && create) {
                info = g_new0(SockInfo, 1);
                info->s = s;
#               ifdef G_OS_WIN32
                        info->io = g_io_channel_win32_new_socket(s);
#               else
                        info->io = g_io_channel_unix_new(s);
#               endif
                info->ev[0].s = info->ev[1].s = info->ev[2].s = s;
                info->ev[0].type = HTEvent_READ;
                info->ev[1].type = HTEvent_WRITE;
                info->ev[2].type = HTEvent_OOB;
                info->ev[0].io_tag = info->ev[1].io_tag = info->ev[2].io_tag = 0;
                info->ev[0].timer_tag = info->ev[1].timer_tag = info->ev[2].timer_tag = 0;
                g_hash_table_insert(sockhash, GINT_TO_POINTER(s), info);
        }
        return info;
}

static gboolean glibwww_timeout_func (gpointer data);
static gboolean glibwww_io_func(GIOChannel *source, GIOCondition condition,
                                gpointer data);

static int
glibwww_event_register (SOCKET s, HTEventType type, HTEvent *event)
{
  //fprintf(stderr, "glibwww_event_register socket=%d type=%d event=%p\n", s, int(type), event);
        SockInfo *info;
        gint priority = G_PRIORITY_DEFAULT;
        GIOCondition condition;

        if (s == INVSOC || HTEvent_INDEX(type) >= HTEvent_TYPES)
                return 0;

        info = get_sock_info(s, TRUE);
        info->ev[HTEvent_INDEX(type)].event = event;

        switch (HTEvent_INDEX(type)) {
        case HTEvent_INDEX(HTEvent_READ):
                condition = static_cast<GIOCondition>(READ_CONDITION); break;
        case HTEvent_INDEX(HTEvent_WRITE):
                condition = static_cast<GIOCondition>(WRITE_CONDITION); break;
        case HTEvent_INDEX(HTEvent_OOB):
                condition = static_cast<GIOCondition>(EXCEPTION_CONDITION); break;
        default:
                g_assert_not_reached ();
                condition = static_cast<GIOCondition>(0); /* this should never occur */
        }
        if (event->priority != HT_PRIORITY_OFF)
                priority = WWW_SCALE_PRIORITY(event->priority);

        if (!info->ev[HTEvent_INDEX(type)].io_tag) {
                info->ev[HTEvent_INDEX(type)].io_tag =
                        g_io_add_watch_full(info->io, priority, condition, glibwww_io_func,
                                            &info->ev[HTEvent_INDEX(type)], NULL);
        }

        if (event->millis >= 0 && !info->ev[HTEvent_INDEX(type)].timer_tag) {
                info->ev[HTEvent_INDEX(type)].timer_tag =
                        g_timeout_add_full(priority, event->millis, glibwww_timeout_func,
                                           &info->ev[HTEvent_INDEX(type)], NULL);
        }

        return HT_OK;
}

static int
glibwww_event_unregister (SOCKET s, HTEventType type)
{
        SockInfo *info = get_sock_info(s, FALSE);
        //fprintf(stderr, "glibwww_event_unregister socket=%d type=%d info=%p\n", s, int(type), info);

        if (info) {
                if (info->ev[HTEvent_INDEX(type)].io_tag)
                        g_source_remove(info->ev[HTEvent_INDEX(type)].io_tag);
                if (info->ev[HTEvent_INDEX(type)].timer_tag)
                        g_source_remove(info->ev[HTEvent_INDEX(type)].timer_tag);

                info->ev[HTEvent_INDEX(type)].event = NULL;
                info->ev[HTEvent_INDEX(type)].io_tag = 0;
                info->ev[HTEvent_INDEX(type)].timer_tag = 0;

#               ifdef G_OS_WIN32
                  /* clean up sock hash if needed */
                  if (info->ev[0].io_tag == 0 && info->ev[1].io_tag == 0
                      && info->ev[2].io_tag == 0) {
                    /*g_message("Freeing sock:%d", s);*/
                    g_hash_table_remove(sockhash, GINT_TO_POINTER(s));
                    g_io_channel_unref(info->io);
                    g_free(info);
                  }
#               endif

                return HT_OK;
        }
        return HT_ERROR;
}

static gboolean
glibwww_timeout_func (gpointer data)
{
        SockEventInfo *info = (SockEventInfo *)data;
        HTEvent *event = info->event;

        if (event)
                (* event->cbf) (info->s, event->param, HTEvent_TIMEOUT);
        return info->timer_tag != 0; /* XXXX a hack */
}

static gboolean
glibwww_io_func(GIOChannel */*source*/, GIOCondition /*condition*/,
                gpointer data)
{
        SockEventInfo *info = (SockEventInfo *)data;
        HTEvent *event = info->event;
        //fprintf(stderr, "glibwww_io_func: event %p, event->cbf %p\n", event, event->cbf);

        if (info->timer_tag) {
                g_source_remove(info->timer_tag);
                info->timer_tag = 0;
        }
        if (event && event->millis >= 0) {
                gint priority = G_PRIORITY_DEFAULT;

                if (event->priority != HT_PRIORITY_OFF)
                        priority = WWW_SCALE_PRIORITY(event->priority);
                info->timer_tag =
                        g_timeout_add_full(priority, info->event->millis, glibwww_timeout_func,
                                           info, NULL);
        }

        if (event)
                (* event->cbf) (info->s, event->param, info->type);
        return info->io_tag != 0; /* XXXX a hack */
}

static GHashTable *timers = NULL;

static gboolean
glibwww_dispatch_timer(gpointer data)
{
        HTTimer *timer = (HTTimer *)data;

        HTTimer_dispatch(timer);
        return FALSE;
}

static BOOL
glibwww_timer_register(HTTimer *timer)
{
        guint tag;

        if (!timers)
                timers = g_hash_table_new(g_direct_hash, g_direct_equal);

        tag = g_timeout_add(HTTimer_expiresRelative(timer),
                            glibwww_dispatch_timer, timer);
        g_hash_table_insert(timers, timer, GUINT_TO_POINTER(tag));
        return YES;
}

static BOOL
glibwww_timer_unregister(HTTimer *timer) {
        guint tag;

        if (!timers)
                return NO;
        tag = GPOINTER_TO_UINT(g_hash_table_lookup(timers, timer));
        if (tag) {
                g_source_remove(tag);
                g_hash_table_remove(timers, timer);
                return YES;
        }
        return NO;
}

void
glibwww_register_callbacks(void)
{
        HTEvent_setRegisterCallback(glibwww_event_register);
        HTEvent_setUnregisterCallback(glibwww_event_unregister);

        HTTimer_registerSetTimerCallback(glibwww_timer_register);
        HTTimer_registerDeleteTimerCallback(glibwww_timer_unregister);
}


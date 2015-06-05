// XXX where does mon_u in u3_unix get initialized?
// XXX _unix_string_to_path
// XXX aww, man, i do need parentage because of the dryness thing
// XXX _unix_dir_watch
// XXX _unix_file_watch
/* v/unix.c
**
**  This file is in the public domain.
*/
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <setjmp.h>
#include <gmp.h>
#include <dirent.h>
#include <stdint.h>
#include <uv.h>
#include <termios.h>
#include <term.h>
#include <errno.h>
#include <libgen.h>

#include "all.h"
#include "v/vere.h"

/* undef this to turn off syncing out to unix */
#define ERGO_SYNC

/* _unix_down(): descend path.
*/
static c3_c*
_unix_down(c3_c* pax_c, c3_c* sub_c)
{
  c3_w pax_w = strlen(pax_c);
  c3_w sub_w = strlen(sub_c);
  c3_c* don_c = c3_malloc(pax_w + strlen(sub_c) + 2);

  strncpy(don_c, pax_c, pax_w + 1);
  don_c[pax_w] = '/';
  strncpy(don_c + pax_w + 1, sub_c, sub_w + 1);
  don_c[pax_w + sub_w + 1] = '\0';

  return don_c;
}

/* _unix_mkdir(): mkdir, asserting.
*/
static void
_unix_mkdir(c3_c* pax_c)
{
  if ( 0 != mkdir(pax_c, 0755) && EEXIST != errno) {
    uL(fprintf(uH, "error mkdiring %s: %s\n", pax_c, strerror(errno)));
    c3_assert(0);
  }
}

/* _unix_opendir(): opendir, recreating if nonexistent.
*/
static DIR*
_unix_opendir(c3_c* pax_c)
{
  DIR* rid_u = opendir(pax_c);

  if ( !rid_u ) {
    // uL(fprintf(uH, "%s: %s\n", pax_c, strerror(errno)));
    _unix_mkdir(pax_c);
    rid_u = opendir(pax_c);
    if ( !rid_u ) {
      uL(fprintf(uH, "%s: %s\n", pax_c, strerror(errno)));
      c3_assert(0);
    }
  }
  return rid_u;
}


/* _unix_get_mount_point(): retrieve or create mount point
*/
static u3_umon*
_unix_get_mount_point(u3_noun mon)
{
  if ( c3n == u3ud(mon) ) {
    c3_assert(!"mount point must be an atom");
    u3z(mon);
    return NULL;
  }

  c3_c* nam_c = u3r_string(mon);
  u3_umon* mon_u;

  for ( mon_u = u3_Host.unx_u.mon_u;
        mon_u && 0 != strcmp(nam_c, mon_u->nam_c);
        mon_u = mon_u->nex_u )
  {
  }

  if ( 0 == u3_mon ) {
    mon_u = malloc(sizeof(u3_umon));
    mon_u->nam_c = nam_c;
    mon_u->dir_u = malloc(sizeof(u3_udir));
    mon_u->dir_u.dir = c3y;
    mon_u->dir_u.dry = c3n;
    mon_u->dir_u.pax_c = _unix_down(u3_Host.dir_c, nam_c);

    /*
    c3_w dir_w = strlen(u3_Host.dir_c) 
    c3_w nam_w = strlen(nam_c);
    mon_u->dir_u.pax_c = malloc(sizeof(c3_c) * (dir_w + 1 + nam_w));
    strncpy(mon_u->dir_u.pax_c, u3_Host.dir_c, dir_w);
    mon_u->dir_u.pax_c[dir_w] = '/';
    strncpy(mon_u->dir_u.pax_c+dir_w+1, nam_c, nam_w);
    */

    mon_u->dir_u.nex_u = NULL;
    mon_u->dir_u.kid_u = NULL;
    mon_u->nex_u = u3_Host.unx_u.mon_u;
    u3_Host.unx_u.mon_u = mon_u;
  }

  free(nam_c);
  u3z(mon);

  return mon_u;
}

/* _unix_free_node(): free node, deleting everything within
*/
void
_unix_free_node(u3_unod* nod_u)
{
  if ( c3y == nod_u->dir_u ) {
    uv_close((uv_handle_t*)&dir_u->was_u, _unix_free_dir);
  }
  else {
    uv_close((uv_handle_t*)&dir_u->was_u, _unix_free_file);
  }
}

/* _unix_free_file(): free file, unlinking it
*/
void
_unix_free_file(u3_ufil* fil_u)
{
  if ( 0 != unlink(fil_u->pax_c) && ENOENT != errno ) {
    uL(fprintf(uH, "error unlinking %s: %s\n", fil_u->pax_c, strerror(errno)));
    c3_assert(0);
  }

  free(nod_u->pax_c);
  free(nod_u);
}

/* _unix_free_dir(): free directory, deleting everything within
*/
static void
_unix_free_dir(uv_handle_t* was_u)
{
  u3_udir* dir_u = (void*) was_u;

  u3_unod nod_u = dir_u->kid_u;
  while ( nod_u ) {
    u3_udir* nex_u = nod_u->nex_u;
    _unix_free_node(nod_u);
    nod_u = nex_u;
  }

  if ( 0 != rmdir(dir_u->pax_c) && ENOENT != errno ) {
    uL(fprintf(uH, "error rmdiring %s: %s\n", dir_u->pax_c, strerror(errno)));
    c3_assert(0);
  }

  free(nod_u->pax_c);
  free(nod_u);
}

/* _unix_free_mount_point(): free mount point
*/
void
_unix_free_mount_point(u3_umon* mon_u)
{
  u3_unod nod_u;
  for ( nod_u = mon_u->dir_u.kid_u; kid_u; kid_u = kid_u->nex_u ) {
    _unix_free_node(kid_u);
  }

  free(mon_u->dir_u.pax_c);
  free(mon_u->nam_c);
  free(mon_u);
}

/* _unix_delete_mount_point(): remove mount point from list and free
*/
void
_unix_delete_mount_point(u3_noun mon)
{
  if ( c3n == u3ud(mon) ) {
    c3_assert(!"mount point must be an atom");
    u3z(mon);
    return;
  }

  c3_c* nam_c = u3r_string(mon);
  u3_umon* mon_u;
  u3_umon* tem_u;

  mon_u = u3_host.unx_u.mon_u;
  if ( !mon_u ) {
    uL(fprintf(uH, "mount point already gone: %s\r\n", nam_c));
    goto _delete_mount_point_out;
  }
  if ( 0 == strcmp(nam_c, mon_u->nam_c) ) {
    u3_Host.unx_u.mon_u = mon_u->nex_u;
    _unix_free_mount_point(mon_u);
    goto _delete_mount_point_out;
  }

  for ( ;
        mon_u->nex_u && 0 != strcmp(nam_c, mon_u->nex_u->nam_c);
        mon_u = mon_u->nex_u )
  {
  }

  if ( !mon_u->nex_u ) {
    uL(fprintf(uH, "mount point already gone: %s\r\n", nam_c));
    goto _delete_mount_point_out;
  }

  tem_u = mon_u->nex_u;
  mon_u->nex_u = mon_u->nex_u->nex_u;
  _unix_free_mount_point(tem_u);

_delete_mount_point_out:
  free(nam_c);
  u3z(mon);
}

/* _unix_node_update(): update node, producing list of changes
*/
static u3_noun
_unix_node_update(u3_unod* nod_u)
{
  if ( c3y == dir_u->dir ) {
    return _unix_dir_update((void*)nod_u);
  }
  else {
    return _unix_file_update((void*)nod_u);
  }
}

/* _unix_file_update(): update file, producing list of changes
 *
 * when scanning through files, if dry, do nothing.  otherwise, mark as
 * dry, then check if file exists.  if not, remove self from node list
 * and add path plus sig to %into event.  otherwise, read the file and
 * get a md5 checksum.  if same as sum_w, move on.  otherwise, overwrite
 * sum_w with new md5sum and add path plus data to %into event.
*/
static u3_noun
_unix_file_update(u3_ufil* fil_u)
{
  c3_assert( c3n == fil_u->dir );

  if ( c3y == fil_u->dry ) {
    return u3_nul;
  }

  fil_u->dry = c3y;

  struct stat buf_u;
  c3_i  fid_i = open(fil_u->pax_c, O_RDONLY, 0644);
  c3_w  len_w;
  c3_y* dat_y;

  if ( (fid_i < 0) || (fstat(fid_i, &buf_u) < 0) ) {
    if ( ENOENT == erno ) {
      return u3nc(u3nc(pax, u3_nul), u3_nul)
    }
    else {
      uL(fprintf(uH, "error opening file %s: %s\r\n",
                 fil_u->pax_c, strerror(errno)));
      return u3_nul;
    }
  }

  len_w = buf_u.st_size;
  dat_y = c3_malloc(len_w);

  red_w = read(fid_i, dat_y, len_w);
  c3_i col_i = close(fid_i);

  if ( col_i < 0 ) {
    uL(fprintf(uH, "error closing file %s: %s\r\n",
               fil_u->pax_c, strerror(errno)));
  }

  if ( len_w != red_w ) {
    free(dat_y);
    c3_assert(0);
    return u3_nul;
  }
  else {
    // XXX check md5
    u3_noun pax = _unix_string_to_path(fil_u->pax_c);
    u3_noun mim = u3nt(c3__text, u3i_string("plain"), u3_nul);
    u3_noun dat = u3nt(mim, len_w, u3i_bytes(len_w, dat_y));
    return u3nc(u3nt(pax, u3_nul, dat), u3_nul)
  }
}

/* _unix_dir_update(): update directory, producing list of changes
*/
static u3_noun
_unix_dir_update(u3_udir* dir_u)
{
  c3_assert( c3y == dir_u->dir );

  if ( c3y == dir_u->dry ) {
    return u3_nul;
  }

  u3_nod* nod_u;

  for ( nod_u = dir_u->kid_u; nod_u; ) {
    if ( c3y == nod_u->dry ) {
      nod_u = nod_u->nex_u;
    }
    else {
      if ( c3y == nod_u->dir ) {
        DIR* red_u = opendir(nod_u->pax_c);

        if ( 0 == red_u ) {
          u3_udir* nex_u = nod_u->nex_u;

          _unix_free_node(nod_u); // XXX actually need to delete from list

          nod_u = nex_u;
        }
        else {
          closedir(rid_u);
          nod_u = nod_u->nex_u;
        }
      }
      else {
      }
    }
  }

  // Check for new files

  DIR* rid_u = opendir(dir_u->pax_c);
  if ( !rid_u ) {
    uL(fprintf(uH, "error opening directory %s: %s\r\n",
               dir_u->pax_c, strerror(errno)));
  }

  while ( 1 ) {
    struct dirent  ent_u;
    struct dirent* out_u;
    c3_w err_w
  
    if ( (err_w = readdir_r(rid_u, &ent_u, &out_u)) != 0 ) {
      uL(fprintf(uH, "error loading directory %s: %s\n",
                 dir_u->pax_c, strerror(err_w)));
      c3_assert(0);
    }
    else if ( !out_u ) {
      break;
    }
    else if ( '.' == out_u->d_name[0] ) {
      continue;
    }
    else {
      c3_c* pax_c = _unix_down(dir_u->pax_c, out_u->d_name);

      struct stat buf_u;

      if ( 0 != stat(pax_c, &buf_u) ) {
        free(pax_c);
        continue;
      }
      else {
        u3_unod* nod_u;
        for ( nod_u = dir_u->kid_u; nod_u; nod_u = nod_u->nex_u ) {
          if ( !strcmp(pax_c, nod_u->pax_c) ) {
            if ( S_ISDIR(buf_u.st_mode) ) {
              c3_assert(nod_u->dir);
            }
            else {
              c3_assert(!nod_u->dir);
            }
            break;
          }
        }

        if ( !nod_u ) {
          if ( !S_ISDIR(buf_u.st_mode) ) {
            if ( !strchr(out_u->d_name,'.')
                 || strchr(out_u->d_name,'.') != strrchr(out_u->d_name,'.')
                 || '~' == out_u->d_name[strlen(out_u->d_name) - 1]
               ) {
              free(pax_c);
              continue;
            }

            u3_ufil* fil_u = c3_malloc(sizeof(u3_ufil));
            _unix_file_watch(fil_u, dir_u, pax_c);
            fil_u->nex = dir_u->kid_u;
            dir_u->kid_u = fil_u;
          }
          else {
            if ( strchr(out_u->d_name,'.') ) {
              free(pax_c);
              continue;
            }

            u3_udir* dis_u = c3_malloc(sizeof(u3_udir));
            _unix_dir_watch(dis_u, dir_u, pax_c);
            _unix_dir_update(dis_u);
            dis_u->nex_u = dir_u->kid_u;
            dir_u->kid_u = dis_u;
          }
        }
      }
    }
  }

  c3_w col_i = closedir(rid_u);
  if ( col_i < 0 ) {
    uL(fprintf(uH, "error closing directory %s: %s\r\n",
               dir_u->pax_c, strerror(errno)));
  }

  // get change list

  u3_noun can;
  u3_unod nod_u;
  for ( nod_u = dir_u->kid_u; nod_u; nod_u = nod_u->nex_u ) {
    can = u3kb_weld(_unix_node_update(nod_u),can);
  }

  return can;
}

/* _unix_mount_update(): update mount point
*/
void
_unix_mount_update(u3_umon* mon_u)
{
  if ( c3n == mon_u->dir_u.dry ) {
    u3_noun can = _unix_dir_update(dir_u);
    u3v_plan(u3nq(u3_blip, c3__sync, u3k(u3A->sen), u3_nul),
             u3nt(c3__into, u3i_string(mon_u->nam_c), can));
  }
}

/* _unix_sync_ergo(): sync changes to unix
*/
void
_unix_sync_ergo(u3_umon mon_u, u3_noun can)
{
  c3_assert(!"not implemented");
  u3z(can);
}

/* _unix_sign_cb: signal callback.
*/
static void
_unix_sign_cb(uv_signal_t* sil_u, c3_i num_i)
{
  u3_lo_open();
  {
    switch ( num_i ) {
      default: fprintf(stderr, "\r\nmysterious signal %d\r\n", num_i); break;

      case SIGTERM:
        fprintf(stderr, "\r\ncaught signal %d\r\n", num_i);
        u3_Host.liv = c3n;
        break;
      case SIGINT: 
        fprintf(stderr, "\r\ninterrupt\r\n");
        u3_term_ef_ctlc(); 
        break;
      case SIGWINCH: u3_term_ef_winc(); break;
    }
  }
  u3_lo_shut(c3y);
}

/* _unix_ef_sync(): check for files to sync.
 */
static void
_unix_ef_sync(uv_check_t* han_u)
{
  u3_lo_open();
  u3_lo_shut(c3y);
}

/* u3_unix_ef_ergo(): update filesystem from urbit
*/
void
u3_unix_ef_ergo(u3_noun mon
                u3_noun can)
{
  u3_uhot* hot_u;

  mon_u = _unix_get_mount_point(mon);

  _unix_sync_ergo(mon_u, can);
}

/* u3_unix_io_init(): initialize unix sync.
*/
void
u3_unix_io_init(void)
{
  u3_unix* unx_u = &u3_Host.unx_u;

  {
    u3_usig* sig_u;

    sig_u = c3_malloc(sizeof(u3_usig));
    uv_signal_init(u3L, &sig_u->sil_u);

    sig_u->num_i = SIGTERM;
    sig_u->nex_u = unx_u->sig_u;
    unx_u->sig_u = sig_u;
  }
  {
    u3_usig* sig_u;

    sig_u = c3_malloc(sizeof(u3_usig));
    uv_signal_init(u3L, &sig_u->sil_u);

    sig_u->num_i = SIGINT;
    sig_u->nex_u = unx_u->sig_u;
    unx_u->sig_u = sig_u;
  }
  {
    u3_usig* sig_u;

    sig_u = c3_malloc(sizeof(u3_usig));
    uv_signal_init(u3L, &sig_u->sil_u);

    sig_u->num_i = SIGWINCH;
    sig_u->nex_u = unx_u->sig_u;
    unx_u->sig_u = sig_u;
  }

  uv_check_init(u3_Host.lup_u, &u3_Host.unx_u.syn_u);
}

/* u3_unix_acquire(): acquire a lockfile, killing anything that holds it.
*/
void
u3_unix_acquire(c3_c* pax_c)
{
  c3_c* paf_c = _unix_down(pax_c, ".vere.lock");
  c3_w pid_w;
  FILE* loq_u;

  if ( NULL != (loq_u = fopen(paf_c, "r")) ) {
    if ( 1 != fscanf(loq_u, "%" SCNu32, &pid_w) ) {
      uL(fprintf(uH, "lockfile %s is corrupt!\n", paf_c));
      kill(getpid(), SIGTERM);
      sleep(1); c3_assert(0);
    }
    else {
      c3_w i_w;

      if ( -1 != kill(pid_w, SIGTERM) ) {
        uL(fprintf(uH, "unix: stopping process %d, live in %s...\n",
                        pid_w, pax_c));

        for ( i_w = 0; i_w < 16; i_w++ ) {
          sleep(1);
          if ( -1 == kill(pid_w, SIGTERM) ) {
            break;
          }
        }
        if ( 16 == i_w ) {
          for ( i_w = 0; i_w < 16; i_w++ ) {
            if ( -1 == kill(pid_w, SIGKILL) ) {
              break;
            }
            sleep(1);
          }
        }
        if ( 16 == i_w ) {
          uL(fprintf(uH, "process %d seems unkillable!\n", pid_w));
          c3_assert(0);
        }
        uL(fprintf(uH, "unix: stopped old process %u\n", pid_w));
      }
    }
    fclose(loq_u);
    unlink(paf_c);
  }

  loq_u = fopen(paf_c, "w");
  fprintf(loq_u, "%u\n", getpid());

  {
    c3_i fid_i = fileno(loq_u);
#if defined(U3_OS_linux)
    fdatasync(fid_i);
#elif defined(U3_OS_osx)
    fcntl(fid_i, F_FULLFSYNC);
#elif defined(U3_OS_bsd)
    fsync(fid_i);
#else
#   error "port: datasync"
#endif
  }
  fclose(loq_u);
  free(paf_c);
}

/* u3_unix_release(): release a lockfile.
*/
void
u3_unix_release(c3_c* pax_c)
{
  c3_c* paf_c = _unix_down(pax_c, ".vere.lock");

  unlink(paf_c);
  free(paf_c);
}

/* u3_unix_ef_move()
*/
void
u3_unix_ef_move(void)
{
  u3_unix* unx_u = &u3_Host.unx_u;
  u3_usig* sig_u;

  for ( sig_u = unx_u->sig_u; sig_u; sig_u = sig_u->nex_u ) {
    uv_signal_start(&sig_u->sil_u, _unix_sign_cb, sig_u->num_i);
  }
}

/* u3_unix_ef_look(): update the root.
*/
void
u3_unix_ef_look(void)
{
  u3_umon* mon_u;

  for ( mon_u = u3_Host.unx_u->mon_u; mon_u; mon_u = mon_u->nex_u ) {
    _unix_mount_update(mon_u);
  }
}

/* u3_unix_io_talk(): start listening for fs events.
*/
void
u3_unix_io_talk()
{
  u3_unix_acquire(u3_Host.dir_c);
  u3_unix_ef_move();
  uv_check_start(&u3_Host.unx_u.syn_u, _unix_ef_sync);
}

/* u3_unix_io_exit(): terminate unix I/O.
*/
void
u3_unix_io_exit(void)
{
  uv_check_stop(&u3_Host.unx_u.syn_u);
  u3_unix_release(u3_Host.dir_c);
}

/* u3_unix_io_poll(): update unix IO state.
*/
void
u3_unix_io_poll(void)
{
}


/* -------------------------------------------------------------------------- *\
 * mod_aclr.c                                                                 *
 * Dmitriy MiksIr <miksir@maker.ru>                                           *
 *                                                                            *
 * Description: http://miksir.pp.ru/mod_aclr/                                 *
 *                                                                            *
 * This program is free software; you can redistribute it and/or              *
 * modify it under the terms of the GNU General Public License                *
 * as published by the Free Software Foundation; either version 2             *
 * of the License, or (at your option) any later version.                     *
 *                                                                            *
 * This program is distributed in the hope that it will be useful,            *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
 * GNU General Public License for more details.                               *
 *                                                                            *
 * You should have received a copy of the GNU General Public License          *
 * along with this program; if not, write to the Free Software                *
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.*
\* -------------------------------------------------------------------------- */

/*
  $Id: mod_aclr.c,v 1.6 2008/07/30 16:02:02 cvs Exp $
*/

#include "httpd.h"
#include "http_core.h"
#define CORE_PRIVATE
#include "http_config.h"
#undef CORE_PRIVATE
#include "http_conf_globals.h"
#include "http_protocol.h"
#include "http_request.h"
#include "http_log.h"
#include "http_main.h"

module aclr_module;

#define ACLR_VERSION "0.04"

#define ACLR_ENABLED 1
#define ACLR_DISABLED 0
#define UNSET -1

typedef struct {
  int state;
  long long fsize;
} aclr_dir_config;

static long long debuglevel = 0;

/* utilites ----------------------------------------- */

static void aclr_debug(int level, server_rec *mainsrv, const char *fmt, ...) {
  char errstr[MAX_STRING_LEN];
  va_list args;
  if (level > debuglevel) return;
  va_start(args, fmt);
  ap_vsnprintf(errstr, sizeof(errstr), fmt, args);
  va_end(args);
  ap_log_error(APLOG_MARK, APLOG_INFO|APLOG_NOERRNO, mainsrv, "[%ld] %s", (long)getpid(), errstr);
  return;
}

long long aclr_parse_bytes(const char *arg) {
  long long ret;
  int arglen = strlen(arg);
  char last_char = arg[arglen - 1];
  int i;
  char arg1[256];
  
  if (arglen >= 256) return(-1);

  if(last_char == 'K' || last_char == 'k'
    || last_char == 'M' || last_char == 'm') {
       arglen --;
  }

  snprintf(arg1,sizeof(arg1),"%s",arg);

  for (i=0;i<arglen;i++) {
    if(!isdigit(arg1[i])) return(-1);
  }

  if(last_char == 'K' || last_char == 'k') 
    return(atoll(arg1) * 1024);  

  if(last_char == 'M' || last_char == 'm')
    return(atoll(arg1) * 1048576);

  return(atoll(arg1));
}

/* runtime ----------------------------------------- */

static int aclr_handler(request_rec *r)
{
  long long	filesize;
  int		rc;
  const char	*idhead;
  char  *idh1;
  char		iredirect[MAX_STRING_LEN];
  aclr_dir_config *cfg = (aclr_dir_config *)ap_get_module_config(r->per_dir_config, &aclr_module);
  const char *server_name    = ap_get_server_name(r);

  if(cfg->state != ACLR_ENABLED)
    return(DECLINED);

  if(!ap_is_initial_req(r)) {
    aclr_debug(4, r->server, "Sub-request pass to core. ://%s%s", server_name, r->uri);
    return(DECLINED);
  }

  if (idhead = ap_table_get(r->headers_in, "X-Accel-Internal")) {
  	 if ((idh1 = strstr(idhead, "%host%"))) {
  	 	  *idh1 = '\0';
  	 	  idh1 += 6;
  	 	  snprintf(iredirect,sizeof(iredirect),"%s%s%s%s",idhead,server_name,idh1,r->uri);
  	 } else {
  	    snprintf(iredirect,sizeof(iredirect),"%s%s",idhead,r->uri);
  	 }
  	 aclr_debug(4, r->server, "Request from frontend received, try to proceed. ://%s%s -> %s", server_name, r->uri, iredirect);
  } else {
  	 aclr_debug(4, r->server, "Request from plain client pass to core. ://%s%s", server_name, r->uri);
     return(DECLINED);
  }

  ap_table_set(r->headers_out, "X-ACLR-Version", ACLR_VERSION);

  if ((rc = ap_discard_request_body(r)) != OK) {
    return rc;
  }

  if (r->method_number != M_GET) {
  	aclr_debug(3, r->server, "Request method is not GET, pass to core. ://%s%s", server_name, r->uri);
    return(DECLINED);
  }

  if (r->finfo.st_mode == 0 || (r->path_info && *r->path_info)) {
  	aclr_debug(3, r->server, "Request file not found, pass to core. ://%s%s", server_name, r->uri);
    return(DECLINED);
  }

  filesize = r->finfo.st_size;
  if (cfg->fsize != UNSET && filesize < cfg->fsize) {
  	aclr_debug(2, r->server, "Request file size (%s) less than minimum size (%s), pass to core. ://%s%s", filesize, cfg->fsize, server_name, r->uri);
    return(DECLINED);
  }

  ap_table_set(r->headers_out, "X-Accel-Redirect", iredirect);

  ap_update_mtime(r, r->finfo.st_mtime);
  if (rc = ap_set_content_length(r, 0))
     return rc;
  ap_send_http_header(r);

  aclr_debug(1, r->server, "Request ://%s%s redirected to %s", server_name, r->uri, iredirect);
  return OK;
}

/* configs ------------------------------------------ */

static void *aclr_create_dir_config(pool *p, char *path) {
  aclr_dir_config *cfg = (aclr_dir_config *)ap_palloc(p, sizeof(aclr_dir_config));
  cfg->state   = UNSET;
  cfg->fsize   = UNSET;
  return( (void *)cfg );
}

/* commands ------------------------------------------ */

static const char *aclr_cmd_state(cmd_parms *parms, void *mconfig, int flag) {
  aclr_dir_config *cfg = (aclr_dir_config *)mconfig;
  cfg->state = (flag ? ACLR_ENABLED : ACLR_DISABLED);
  return(NULL);
}

static const char *aclr_cmd_size(cmd_parms *parms, void *mconfig, const char *arg) {
  aclr_dir_config *cfg = (aclr_dir_config *)mconfig;
  long long bytes = aclr_parse_bytes(arg);
  
  if (bytes >= 0) {
     cfg->fsize = bytes;
  } else {
     cfg->fsize = UNSET;
     ap_log_error(APLOG_MARK, APLOG_ERR|APLOG_NOERRNO, parms->server,  "Argument %s of AccelRedirectSize is not number", arg);
  }

  return(NULL);
}

static const char *aclr_cmd_debug(cmd_parms *parms, void *mconfig, const char *arg)
{
  const char *err = ap_check_cmd_context(parms, GLOBAL_ONLY);
  long long level;
  if (err != NULL) {
    return err;
  }
  level = aclr_parse_bytes(arg);

  if (level >= 0) {
     debuglevel = level;
     aclr_debug(1, parms->server, "AccelRedirectDebug %d", debuglevel);
  } else {
     ap_log_error(APLOG_MARK, APLOG_ERR|APLOG_NOERRNO, parms->server,  "Argument %s of AccelRedirectDebug is not number", arg);
  }

  return(NULL);
}

static const command_rec aclr_cmds[] = {
  { "AccelRedirectSet", aclr_cmd_state, NULL, ACCESS_CONF | RSRC_CONF, FLAG,
    "turn X-Accel-Redirect support On or Off (default Off)" },
  { "AccelRedirectSize", aclr_cmd_size, NULL, ACCESS_CONF | RSRC_CONF, TAKE1,
    "minimum size of file for redirect in bytes" },
  { "AccelRedirectDebug", aclr_cmd_debug, NULL, RSRC_CONF,  TAKE1,
    "level of debug, 0 is turn debug off" },
  { NULL }
};

/* module info ------------------------------------------- */

static const handler_rec aclr_handlers[] = {
  { "*/*",          aclr_handler },
  { NULL }
};

module MODULE_VAR_EXPORT aclr_module = {
  STANDARD_MODULE_STUFF,
  NULL,                       /* module initializer                  */
  aclr_create_dir_config,     /* create per-dir    config structures */
  NULL,                       /* merge  per-dir    config structures */
  NULL,                       /* create per-server config structures */
  NULL,                       /* merge  per-server config structures */
  aclr_cmds,                  /* table of config file commands       */
  aclr_handlers,              /* [#8] MIME-typed-dispatched handlers */
  NULL,                       /* [#1] URI to filename translation    */
  NULL,                       /* [#4] validate user id from request  */
  NULL,                       /* [#5] check if the user is ok _here_ */
  NULL,                       /* [#3] check access by host address   */
  NULL,                       /* [#6] determine MIME type            */
  NULL,  	                    /* [#7] pre-run fixups                 */
  NULL,                       /* [#9] log a transaction              */
  NULL,                       /* [#2] header parser                  */
  NULL,                       /* child_init                          */
  NULL,                       /* child_exit                          */
  NULL                        /* [#0] post read-request              */
};

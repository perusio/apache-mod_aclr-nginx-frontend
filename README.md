# Apache module for easy setup of Nginx as a frontend to Apache

## Introduction

`mod_aclr` &mdash; as in **ac**ce**l r**edirect &mdash; is an module
for Apache, currently only 1.3 is supported, that makes very easy to
setup [Nginx](http://wiki.nginx.org) as frontend to Apache for serving
static files and thus take full benefit of its speed while retaining
the *"convenience"* of things like `.htaccess`, which are a
requirement for shared hosting support.


The [author](http://miksir.livejournal.com/) of the module no longer
provides any support of this module, although still providing the code
and the [documentation](http://miksir.maker.ru/?r=72).

The module creates a special handler for Apache that sends Nginx a
special header
[`X-Accel-Redirect`](http://wiki.nginx.org/X-accel#X-Accel-Redirect)
with the location of the static file to be served. If the request is
dynamic, i.e., to be handled by a embedded language module or by a
FastCGI backend, the request stays within Apache.

## mod_aclr directives

 **AccelRedirectSet** [On|Off]<br>
 Default: Off<br>
 Context: server config, virtual host, directory

 This directive enables the module.

 **AccelRedirectSize** [size] [k|M]<br>
 Default: -1<br>
 Context: server config, virtual host, directory
 
 This directive sets the minimum size of the static files for which the
 request will be handled by Nginx. The default `-1` means that **all**
 static files will be handled by Nginx, i.e., Apache will send the
 `X-Accel-Redirect` to Nginx for all static files.

 **AccelRedirectDebug** [0-4]<br>
 Default: 0<br>
 Context: server config

 Sets the debug level for this module.

## Apache: mod_aclr installation and configuration

 1. Compile the module using [`apxs`](http://man.cx/apxs).
     
        apxs -c mod_aclr.c
    
 2. Install it.
     
        apxs -i mod_aclr.so 
 
 4. Configure the module in the Apache configuration file `httpd.conf`
    or equivalent. For example:
 
        AccelRedirectSet On
        AccelRedirectSize 1k
     

 3. This module **must** be the **first** to be loaded:
 
        LoadModule aclr_module libexec/mod_aclr.so
        AddModule mod_aclr.c

 4. Reload Apache.
 
 5. Done.

## mod_aclr: Nginx configuration

The configuration relies on the fact that `mod_aclr` **requires** the
header `X-Accel-Internal` to be present.

 1. Proxy configuration:
        
        location / {
            proxy_pass          http://127.0.0.1:80;
            proxy_set_header    X-Real-IP  $remote_addr;
            ## X-Accel-Internal is set to /static-internal.
            proxy_set_header    X-Accel-Internal /static-internal;
            proxy_set_header    Host $http_host;
        }

     The location `/static-internal` is the one that will handle the
     static file serving by Nginx.
     
     
 2. Static file handling location:
  
        location ~* /static-internal/(?<requested_asset>*.)$ {
           ## Path to the Apache root directory.
           root /var/www/mysite.com;
           return 301 /$requested_asset;
           ## Protect direct access to this location.
           internal;
         }

     `mod_aclr` returns the header `X-Accel-Redirect:
     /static-internal/path/to/file` upon which Nginx serves the file
      directly.
      
## How it works

The content handler provided by `mod_aclr` runs after all other
handlers, meaning that all other content handlers have already run and
therefore the request must be a static file to be served.     

It checks if the `X-Accel-Internal` header is present. If it is and if
the requested file is static, it replies with the `X-Accel-Redirect`
header with the file location and a `Content-Length: 0` header. 
     
The fact that the presence of the `X-Accel-Internal` is required
enables you to control precisely when to get Nginx to serve the static
file or go directly to the backend.
     
## Troubleshooting

If your site only serves static files then this means that `mod_aclr`
is not the **first** module to be loaded. `mod_aclr` **must** be the
**first** module to be loaded in your `httpd.conf` file.

## Acknowledgments

This project came to my attention through a
[discussion](http://mailman.nginx.org/pipermail/nginx-ru/2011-August/042282.html)
on the
[russian Nginx mailing list](http://mailman.nginx.org/pipermail/nginx-ru/).

## TODO

Update this module to the Apache 2.2 API.

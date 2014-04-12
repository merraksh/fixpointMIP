/*
 * Name:    cmdline.h
 * Author:  Pietro Belotti
 *
 * This code is published under the Eclipse Public License (EPL).
 * See http://www.eclipse.org/legal/epl-v10.html
 *
 */

#ifndef PAR_H
#define PAR_H

#define CSTR() (char *)

enum etype {TTOGGLE, TCHARA, TINT, TDOUBLE, TSTRING};

/** \struct tpar
 *  \brief argv option struct
 *
 *  For each program argument there is a short/long option string,
 *  (possible) the address of a variable to modify on finding this
 *  option, and a help message to be displayed on "--help".
 */

typedef struct {

  char shortopt;   /**< Short option character */
  char *longopt;   /**< Long option string     */

  double initial;  /**< Option's default value */

  void *par;       /**< Address of the changed parameter (may be NULL)  */
  enum etype type; /**< What type is the option? [toggle|int|double...] */

  char *help;      /**< A help message */

} tpar;

char **readargs (int, char **, tpar *);        /* parse command line arguments */

int act (char ***, int *, enum etype, void *); /* type-based parameter assigned */

void print_help (char *, tpar *);              /* print help message */

void set_default_args (tpar *);                /* set default values in options */

#if defined(_MSC_VER)
#define Strcpy strcpy_s
#define Sprintf sprintf_s
// Turn off compiler warning about long names
#  pragma warning(disable:4786)
#else
#define Strcpy(a,b,c) strncpy(a,c,b)
#define Sprintf sprintf
#endif

#endif

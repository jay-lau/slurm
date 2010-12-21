/*****************************************************************************\
 *  slurm_rlimits_info.c - resource limits that are used by srun and the slurmd
 *  $Id: slurm.hp.rlimits.patch,v 1.5 2005/07/18 18:39:11 danielc Exp $
 *****************************************************************************
 *
 *  Copyright (C) 2005 Hewlett-Packard Development Company, L.P.
 *
 *  This file is part of SLURM, a resource management program.
 *  For details, see <http://www.llnl.gov/linux/slurm/>.
 *
 *  SLURM is free software; you can redistribute it and/or modify it under
 *  the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version.
 *
 *  SLURM is distributed in the hope that it will be useful, but WITHOUT ANY
 *  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 *  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 *  details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with SLURM; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA  02111-1307  USA.
\*****************************************************************************/

#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <sys/resource.h>

#include "src/common/macros.h"
#include "src/common/xstring.h"
#include "src/common/xassert.h"
#include "src/common/xmalloc.h"
#include "src/common/log.h"
#include "src/common/slurm_rlimits_info.h"


/*
 * List SLURM rlimits get/setrlimit resource number with associated name and
 * whether it should be propagated.
 */

static slurm_rlimits_info_t rlimits_info[] = {

		      /*  resource,        name,       propagate_flag  */

#ifdef RLIMIT_CPU
			{ RLIMIT_CPU,      "CPU",      -1      },
#endif
#ifdef RLIMIT_FSIZE
			{ RLIMIT_FSIZE,    "FSIZE",    -1      },
#endif
#ifdef RLIMIT_DATA
			{ RLIMIT_DATA,     "DATA",     -1      },
#endif
#ifdef RLIMIT_STACK
			{ RLIMIT_STACK,    "STACK",    -1      },
#endif
#ifdef RLIMIT_CORE
			{ RLIMIT_CORE,     "CORE",     -1      },
#endif
#ifdef RLIMIT_RSS
			{ RLIMIT_RSS,      "RSS",      -1      },
#endif
#ifdef RLIMIT_NPROC
			{ RLIMIT_NPROC,    "NPROC",    -1      },
#endif
#ifdef RLIMIT_NOFILE
			{ RLIMIT_NOFILE,   "NOFILE",   -1      },
#endif
#ifdef RLIMIT_MEMLOCK
			{ RLIMIT_MEMLOCK,  "MEMLOCK",  -1      },
#endif
#ifdef RLIMIT_AS
			{ RLIMIT_AS,       "AS",       -1      },
#endif
			{ 0,               NULL,       -1      }
};


static bool rlimits_were_parsed = FALSE;

/*
 * Return a pointer to the private rlimits info array.
 */
slurm_rlimits_info_t *
get_slurm_rlimits_info( void )
{
	xassert( rlimits_were_parsed == TRUE );

	return rlimits_info;
}


#define RLIMIT_         "RLIMIT_"
#define LEN_RLIMIT_     (sizeof( RLIMIT_ ) - 1)
#define RLIMIT_DELIMS   ", \t\n"
/*
 * Parse a comma separated list of RLIMIT names.
 *
 * Return 0 on success, or -1 if the 'rlimits_str' input parameter contains
 * a name that is not in rlimits_info[].
 */
int
parse_rlimits( char *rlimits_str, int propagate_flag )
{
	slurm_rlimits_info_t *rli;	/* ptr iterator for rlimits_info[] */
        char		     *tp;	/* token ptr */
        bool		     found;
	char		     *rlimits_str_dup;

	xassert( rlimits_str );

        if (strcmp( rlimits_str, "ALL" ) == 0) {
                /*
                 * Propagate flag value applies to all rlimits
                 */
                for (rli = rlimits_info; rli->name; rli++)
			rli->propagate_flag = propagate_flag;
		rlimits_were_parsed = TRUE;
                return( 0 );
        }

	/*
	 * Since parse_rlimits may be called multiple times, we 
	 * need to reinitialize the propagate flags when individual
	 * rlimits are specified.
	 */
	if (rlimits_were_parsed)
		for (rli = rlimits_info; rli->name; rli++)
			rli->propagate_flag = -1;

        rlimits_str_dup = xstrdup( rlimits_str );
        if ((tp = strtok( rlimits_str_dup, RLIMIT_DELIMS )) != NULL) {
                do {
                        found = FALSE;
                        for (rli = rlimits_info; rli->name; rli++) {
                                /*
                                 * Accept either "RLIMIT_CORE" or "CORE"
                                 */
                                if (strncmp( tp, RLIMIT_, LEN_RLIMIT_ ) == 0)
                                        tp += LEN_RLIMIT_;
                                if (strcmp( tp, rli->name ))
                                        continue;
                                rli->propagate_flag = propagate_flag;
                                found = TRUE;
                                break;
                        }
                        if (found == FALSE) {
                                error( "Bad rlimit name: %s\n", tp );
				xfree( rlimits_str_dup );
                                return( -1 );
                        }
                } while ((tp = strtok( NULL, RLIMIT_DELIMS )));
        }
	xfree( rlimits_str_dup );

        /*
         * Any rlimits that weren't in the 'rlimits_str' parameter get the
	 * opposite propagate flag value.
         */
	for (rli = rlimits_info; rli->name; rli++)
		if (rli->propagate_flag == -1)
		  rli->propagate_flag = ( ! propagate_flag );

	rlimits_were_parsed = TRUE;
	return( 0 );
}
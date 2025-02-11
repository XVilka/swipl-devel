/*  Part of SWI-Prolog

    Author:        Jan Wielemaker
    E-mail:        J.Wielemaker@vu.nl
    WWW:           http://www.swi-prolog.org
    Copyright (c)  2016-2019, VU University Amsterdam
			      CWI, Amsterdam
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:

    1. Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.

    2. Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in
       the documentation and/or other materials provided with the
       distribution.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
    FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
    COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
    INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
    BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
    CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
    LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
    ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
    POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef _PL_TABLING_H
#define _PL_TABLING_H
#include "pl-trie.h"

typedef enum
{ CLUSTER_ANSWERS,
  CLUSTER_SUSPENSIONS
} cluster_type;

#define DELAY_MAGIC	0x67e9124d
#define WORKLIST_MAGIC	0x67e9124e
#define COMPONENT_MAGIC	0x67e9124f


		 /*******************************
		 *     GLOBAL ENTRY POINTS	*
		 *******************************/

typedef struct worklist_set
{ buffer members;
} worklist_set;

typedef struct component_set
{ buffer members;
} component_set;

typedef enum
{ SCC_ACTIVE=0,
  SCC_MERGED,
  SCC_COMPLETED
} scc_status;

typedef enum
{ SCC_NEG_NONE=0,				/* no negative nodes */
  SCC_NEG_DELAY,
  SCC_NEG_SIMPLIFY
} scc_neg_status;

typedef struct tbl_component
{ int			magic;			/* COMPONENT_MAGIC */
  scc_status	        status;			/* SCC_* */
  scc_neg_status	neg_status;		/* SCC_NEG_* */
  size_t		simplifications;        /* # simplifications */
  struct tbl_component *parent;
  component_set        *children;		/* Child components */
  component_set        *merged;			/* Child components */
  worklist_set         *worklist;		/* Worklist of current query */
  worklist_set         *created_worklists;	/* Worklists created */
  worklist_set	       *delay_worklists;	/* Worklists in need for delays */
  trie		       *leader;			/* Leading variant */
} tbl_component;


		 /*******************************
		 *	   TABLE WORKLIST	*
		 *******************************/

typedef struct cluster
{ cluster_type type;
  struct cluster *next;
  struct cluster *prev;
  buffer members;
} cluster;

typedef struct worklist
{ cluster      *head;			/* answer and dependency clusters */
  cluster      *tail;
  cluster      *riac;			/* rightmost inner answer cluster */
  cluster      *free_clusters;		/* clusters to reuse */
  int		magic;			/* WORKLIST_MAGIC */
  unsigned	completed : 1;		/* Really completed */
  unsigned	ground : 1;		/* Ground call (early completion) */
  unsigned	executing : 1;		/* $tbl_wkl_work/3 in progress */
  unsigned	in_global_wl : 1;	/* already in global worklist */
  unsigned	negative : 1;		/* this is a suspended negation */
  unsigned	neg_delayed : 1;	/* Negative node was delayed */
  unsigned	has_answers : 1;	/* At least one unconditional answer */
  unsigned	answer_completed : 1;	/* Is answer completed */
  unsigned	depend_abolish : 1;	/* Scheduled for depending abolish */
  unsigned	abolish_on_complete : 1;/* Abolish the table on completion */
  size_t	undefined;		/* #undefined answers */

  tbl_component*component;		/* component I belong to */
  trie	       *table;			/* My answer table */

  buffer	delays;			/* Delayed answers */
  buffer	pos_undefined;		/* Positive undefined */
} worklist;


		 /*******************************
		 *	    DELAY LISTS		*
		 *******************************/

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
A `delay_list` represents a conjunction of conditions. Each condition is
either a negative literal <~L> or  a   positive  literal  with an answer
<L,A>.

Delay lists are associated with an answer. An   answer can have a set of
delay lists (`delay_list_set`) and are combined in a `delay_info` struct
that provides information  about  the  variant   for  which  this  is  a
condition.

A `worklist` points at  the  delay  list   elements  for  which  it is a
condition. After resolving a worklist (answer  for positive literals) we
should go over the places where  its   associated  variant  is used as a
condition and

  - Delete the condition from the delay list if the condition is
    satified. If all conditions are satified propagate using the
    resolved answer.
  - Delete the answer from the answer trie and propage that.
- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

typedef struct delay_usage
{ buffer	     answers;		/* trie_node * to conditional answers */
} delay_usage;

typedef struct delay
{ trie	            *variant;		/* Answer trie */
  trie_node         *answer;		/* Answer in there (NULL for negative) */
} delay;

typedef struct delay_set
{ unsigned	     offset;		/* offset in delays */
  unsigned	     size;		/* size of the conjunction */
  unsigned	     active;		/* active members of conjunction */
} delay_set;

typedef struct delay_info
{ trie_node      *variant;		/* Variant trie node */
  unsigned	  has_share_records;	/* We have variable sharing records */
  buffer          delay_sets;		/* The disjunctive conditions */
  buffer	  delays;		/* Store for the delays */
} delay_info;


		 /*******************************
		 *	       IDG		*
		 *******************************/

typedef struct idg_node
{ Table		affected;		/* parent nodes */
  Table		dependent;		/* childs */
  trie	       *atrie;			/* answer trie */
  size_t	answer_count;		/* #answers in previous complete state */
  unsigned	new_answer : 1;		/* Update generated a new answer */
  unsigned	reevaluating : 1;	/* currently re-evaluating */
  int		falsecount;		/* Invalidate count */
} idg_node;


		 /*******************************
		 *	     PROTOTYPES		*
		 *******************************/

COMMON(void)	clearThreadTablingData(PL_local_data_t *ld);
COMMON(term_t)	init_delay_list(void);
COMMON(void)	tbl_push_delay(atom_t atrie, Word wrapper,
			       trie_node *answer ARG_LD);
COMMON(int)	answer_is_conditional(trie_node *answer);

#endif /*_PL_TABLING_H*/

#!/usr/bin/perl
#-------------------------------------------------------------------------
#
# mtlocal.pl--
#    perl script which converts files generated by YACC(BISON) and LEX(FLEX) to 
#    be used in MT version.
#
# NOTES
#    Add THR_LOCAL for generated global and static variables. There are four 
#    parts need to be converted:
#      1. replication parser: (src/backend/replication/)
#                     repl_gram.c repl_scanner.c
#      2. bootstrap parser: (src/backend/bootstrap/)
#                     bootparse.c bootscanner.c
#      3. plsql parser: (src/pl/plpgsql/src/) pl_gram.c pl_gram.h
#
#   sql parser: (src/backend/parser) gram.y scan.l is a reentrant (thread-safe) parser already
#
# USAGE NOTICE
#    1. This script only convert variables generated by YACC and LEX (with
#       prefix "yy"). Global or static variables added in .y and .l files 
#       should be converted manully.
#    2. "THR_LOCAL"s are added to declaration and definition of global or static
#       variables.
#    3. Self-defined global or static variables should be added to .y and .l 
#       files, but not generated .c and .h files. "THR_LOCAL" should be added to
#       declaration and definition.
#       For example:
#            "static int xcdepth = 0;"  ==>  "static THR_LOCAL int xcdepth = 0;"
#            "extern char *buf;"  ==>  "extern THR_LOCAL char *buf;"
#
# IDENTIFICATION
#    $PostgreSQL: pgsql/src/mtlocal.pl,v 1.1 2006/11/29 19:49:31 tgl Exp $
#
#-------------------------------------------------------------------------

use strict;
use warnings;

my $fname = shift;
my $tmpfname = $fname . ".tmp";
my $F;
my $OF;

open($F, $fname) || die "Could not open file $fname\n";
open($OF, ">$tmpfname") || die "Could not open file $tmpfname\n";

while (my $indata = <$F>)
{
	$indata =~ s/(char \*yy_c_buf_p = \(char \*\) 0;)/THR_LOCAL $1/;
	$indata =~ s/(char \*yy_last_accepting_cpos;)/THR_LOCAL $1/;
	$indata =~ s/(char \*yytext;)/THR_LOCAL $1/;
	$indata =~ s/(char \*base_yytext;)/THR_LOCAL $1/;
	$indata =~ s/(char \*plpgsql_base_yytext;)/THR_LOCAL $1/;
	$indata =~ s/(char yy_hold_char;)/THR_LOCAL $1/;
	$indata =~ s/(FILE \*yyin = \(FILE \*\) 0, \*yyout = \(FILE \*\) 0;)/THR_LOCAL $1/;
	$indata =~ s/(FILE \*yyin, \*yyout;)/THR_LOCAL $1/;
	$indata =~ s/(int \*yy_start_stack = 0;)/THR_LOCAL $1/;
	$indata =~ s/(int yy_did_buffer_switch_on_eof;)/THR_LOCAL $1/;
	$indata =~ s/(int yy_init = 1;)/THR_LOCAL $1/;
	$indata =~ s/(int yy_start = 0;)/THR_LOCAL $1/;
	$indata =~ s/(int yy_start_stack_depth = 0;)/THR_LOCAL $1/;
	$indata =~ s/(int yy_start_stack_ptr = 0;)/THR_LOCAL $1/;
	$indata =~ s/(int yychar;)/THR_LOCAL $1/;
	$indata =~ s/(int yydebug;)/THR_LOCAL $1/;
	$indata =~ s/(int yyleng;)/THR_LOCAL $1/;
	$indata =~ s/(int yynerrs;)/THR_LOCAL $1/;
	$indata =~ s/(YY_BUFFER_STATE yy_current_buffer = 0;)/THR_LOCAL $1/;
	$indata =~ s/(yy_state_type yy_last_accepting_state;)/THR_LOCAL $1/;
	$indata =~ s/(YYLTYPE base_yylloc;)/THR_LOCAL $1/;
	$indata =~ s/(YYLTYPE yylloc;)/THR_LOCAL $1/;
	$indata =~ s/(YYSTYPE base_yylval;)/THR_LOCAL $1/;
	$indata =~ s/(YYSTYPE boot_yylval;)/THR_LOCAL $1/;
	$indata =~ s/(YYSTYPE plpgsql_yylval;)/THR_LOCAL $1/;
	$indata =~ s/(YYLTYPE plpgsql_yylloc;)/THR_LOCAL $1/;
	$indata =~ s/(YYSTYPE yylval;)/THR_LOCAL $1/;
	$indata =~ s/(static )(int yy_n_chars;)/$1THR_LOCAL $2/;
	$indata =~ s/(extern )(FILE \*replication_yyin, \*replication_yyout;)/$1THR_LOCAL $2/;
	$indata =~ s/(extern )(int replication_yy_flex_debug;)/$1THR_LOCAL $2/;
	$indata =~ s/(int replication_yyleng;)/THR_LOCAL $1/;
	$indata =~ s/(static )(size_t yy_buffer_stack_top = 0;)/$1THR_LOCAL $2/;
	$indata =~ s/(static )(size_t yy_buffer_stack_max = 0;)/$1THR_LOCAL $2/;
	$indata =~ s/(static )(YY_BUFFER_STATE \* yy_buffer_stack = 0;)/$1THR_LOCAL $2/;
	$indata =~ s/(static )(int yy_init = 0;)/$1THR_LOCAL $2/;
	$indata =~ s/(FILE \*replication_yyin = \(FILE \*\) 0, \*replication_yyout = \(FILE \*\) 0;)/THR_LOCAL $1/;
	$indata =~ s/(int replication_yy_flex_debug = 0;)/THR_LOCAL $1/;
	$indata =~ s/(char \*replication_yytext;)/THR_LOCAL $1/;
	$indata =~ s/(FILE \*yyin = NULL, \*yyout = NULL;)/THR_LOCAL $1/;
	$indata =~ s/(YYLTYPE yylloc)/THR_LOCAL $1/;
	$indata =~ s/(extern )(YYSTYPE replication_yylval;)/$1THR_LOCAL $2/;
	$indata =~ s/(YYSTYPE yylval YY_INITIAL_VALUE\(yyval_default\);)/THR_LOCAL $1/;
	$indata =~ s/THR_LOCAL THR_LOCAL/THR_LOCAL/;
	print $OF $indata;
}
close($F);
close($OF);

unlink($fname);
rename($tmpfname, $fname);

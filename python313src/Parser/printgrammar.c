
/* Print a bunch of C initializers that represent a grammar */

#include "pgenheaders.h"
#include "grammar.h"

/* Forward */
static void printarcs(int, dfa *, FILE *);
static void printstates(grammar *, FILE *);
static void printdfas(grammar *, FILE *);
static void printlabels(grammar *, FILE *, FILE *);

void
printgrammar(grammar *g, FILE *fp, FILE *fp2)
{
    fprintf(fp, "/* Generated by Parser/pgen */\n\n");
    fprintf(fp, "/* wufuheng@gmail.com */\n\n");
    fprintf(fp, "#include \"pgenheaders.h\"\n");
    fprintf(fp, "#include \"grammar.h\"\n");
    fprintf(fp, "PyAPI_DATA(grammar) _PyParser_Grammar;\n");
    printdfas(g, fp);
    printlabels(g, fp, fp2);
    fprintf(fp, "grammar _PyParser_Grammar = {\n");
    fprintf(fp, "    %d,\n", g->g_ndfas);
    fprintf(fp, "    dfas,\n");
    fprintf(fp, "    {%d, labels},\n", g->g_ll.ll_nlabels);
    fprintf(fp, "    %d\n", g->g_start);
    fprintf(fp, "};\n");
}

void
printnonterminals(grammar *g, FILE *fp)
{
    dfa *d;
    int i;

    fprintf(fp, "/* Generated by Parser/pgen */\n\n");

    d = g->g_dfa;
    for (i = g->g_ndfas; --i >= 0; d++)
        fprintf(fp, "#define %s %d\n", d->d_name, d->d_type);
}

static void
printarcs(int i, dfa *d, FILE *fp)
{
    arc *a;
    state *s;
    int j, k;

    s = d->d_state;
    for (j = 0; j < d->d_nstates; j++, s++) {
        fprintf(fp, "static arc arcs_%d_%d[%d] = {\n",
            i, j, s->s_narcs);
        a = s->s_arc;
        for (k = 0; k < s->s_narcs; k++, a++)
            fprintf(fp, "    {%d, %d},\n", a->a_lbl, a->a_arrow);
        fprintf(fp, "};\n");
    }
}

static void
printstates(grammar *g, FILE *fp)
{
    state *s;
    dfa *d;
    int i, j;

    d = g->g_dfa;
    for (i = 0; i < g->g_ndfas; i++, d++) {
        printarcs(i, d, fp);
        fprintf(fp, "static state states_%d[%d] = {\n",
            i, d->d_nstates);
        s = d->d_state;
        for (j = 0; j < d->d_nstates; j++, s++)
            fprintf(fp, "    {%d, arcs_%d_%d},\n",
                s->s_narcs, i, j);
        fprintf(fp, "};\n");
    }
}

static void
printdfas(grammar *g, FILE *fp)
{
    dfa *d;
    int i, j;

    printstates(g, fp);
    //fprintf(fp, "static dfa dfas[%d] = {\n", g->g_ndfas);
    fprintf(fp, "dfa dfas[%d] = {\n", g->g_ndfas);//fuheng
    d = g->g_dfa;
    for (i = 0; i < g->g_ndfas; i++, d++) {
        fprintf(fp, "    {%d, \"%s\", %d, %d, states_%d,\n",
            d->d_type, d->d_name, d->d_initial, d->d_nstates, i);
        fprintf(fp, "     \"");
        for (j = 0; j < NBYTES(g->g_ll.ll_nlabels); j++)
            fprintf(fp, "\\%03o", d->d_first[j] & 0xff);
        fprintf(fp, "\"},\n");
    }
    fprintf(fp, "};\n");
}

static void
printlabels(grammar *g, FILE *fp, FILE *fp2)
{
    label *l;
    int i;

    int nstr=0;
    //fprintf(fp, "static label labels[%d] = {\n", g->g_ll.ll_nlabels);
    fprintf(fp, "label labels[%d] = {\n", g->g_ll.ll_nlabels); // fuheng
    l = g->g_ll.ll_label;
    for (i = g->g_ll.ll_nlabels; --i >= 0; l++) {
        if (l->lb_str == NULL){
            fprintf(fp, "    {%d, 0},//%d\n", l->lb_type, g->g_ll.ll_nlabels - i-1);
        }else{
            fprintf(fp, "    {%d, \"%s\"},//%d\n", l->lb_type, l->lb_str, g->g_ll.ll_nlabels - i-1);
            if (strcmp(l->lb_str,"EMPTY")!=0){
                ++nstr;
            }
        }
    }
    fprintf(fp, "};\n\n");

    //////////////////////////////////////////////////////////////////////////
    fprintf(fp2, "#include \"grammar.h\"\n\n");
    fprintf(fp2, "unsigned short type2label[%d+256] = {\n", g->g_ndfas); // fuheng
    l = g->g_ll.ll_label;
    unsigned short *tmp1 = calloc(g->g_ndfas+256,sizeof(unsigned short));
    label* tmp2 = malloc(nstr*sizeof(label));
    for (i = g->g_ll.ll_nlabels; --i >= 0; l++) {
        int ilabel = g->g_ll.ll_nlabels - i - 1;
        if (l->lb_str == NULL){
            tmp1[l->lb_type] = ilabel;
            //fprintf(fp, "{%d, 0},//%d\n", l->lb_type, g->g_ll.ll_nlabels - i - 1);
        }else{
            if (strcmp(l->lb_str, "EMPTY") != 0){
                static j = 0;
                tmp2[j].lb_type = ilabel;
                tmp2[j++].lb_str = l->lb_str;
            }
        }
    }
    for (i = 0; i < g->g_ndfas+256;++i){
        fprintf(fp2, "%d,", tmp1[i]);
    }
    fprintf(fp2, "};\n\n");

    fprintf(fp2, "#define labelindex2string label\n"
        "labelindex2string type2label2[%d] = { \n",nstr);
    for (i = 0; i < nstr; ++i){
        fprintf(fp2, "{%d, \"%s\"},\n", tmp2[i].lb_type, tmp2[i].lb_str);
    }
    fprintf(fp2, "};\n");
    fprintf(fp2, "\n#define SPEEDUPPARSE\n\
    /*#define ENDMARKER	0\n\
    #define NAME		1\n\
    #define NUMBER		2\n\
    #define STRING		3\n\
    #define NEWLINE		4\n\
    #define INDENT		5\n\
    #define DEDENT		6\n\
    #define LPAR		7\n\
    #define RPAR		8\n\
    #define LSQB		9\n\
    #define RSQB		10\n\
    #define COLON		11\n\
    #define COMMA		12\n\
    #define SEMI		13\n\
    #define PLUS		14\n\
    #define MINUS		15\n\
    #define STAR		16\n\
    #define SLASH		17\n\
    #define VBAR		18\n\
    #define AMPER		19\n\
    #define LESS		20\n\
    #define GREATER		21\n\
    #define EQUAL		22\n\
    #define DOT		23\n\
    #define PERCENT		24\n\
    //#define BACKQUOTE	25\n\
    #define LBRACE		26\n\
    #define RBRACE		27\n\
    #define EQEQUAL		28\n\
    #define NOTEQUAL	29\n\
    #define LESSEQUAL	30\n\
    #define GREATEREQUAL	31\n\
    #define TILDE		32\n\
    #define CIRCUMFLEX	33\n\
    #define LEFTSHIFT	34\n\
    #define RIGHTSHIFT	35\n\
    #define DOUBLESTAR	36\n\
    #define PLUSEQUAL	37\n\
    #define MINEQUAL	38\n\
    #define STAREQUAL	39\n\
    #define SLASHEQUAL	40\n\
    #define PERCENTEQUAL	41\n\
    #define AMPEREQUAL	42\n\
    #define VBAREQUAL	43\n\
    #define CIRCUMFLEXEQUAL	44\n\
    #define LEFTSHIFTEQUAL	45\n\
    #define RIGHTSHIFTEQUAL	46\n\
    #define DOUBLESTAREQUAL	47\n\
    #define DOUBLESLASH	48\n\
    #define DOUBLESLASHEQUAL 49\n\
    #define AT              50	\n\
    #define RARROW          51\n\
    #define ELLIPSIS        52\n\
    #define OP		53\n\
    #define ERRORTOKEN	54\n\
    #define N_TOKENS	55\n\
    #define NT_OFFSET		256\n\
    #define ISTERMINAL(x)		((x) < NT_OFFSET)\n\
    #define ISNONTERMINAL(x)	((x) >= NT_OFFSET)\n\
    #define ISEOF(x)		((x) == ENDMARKER)*/\n");
    free(tmp1);
    free(tmp2);
    
}

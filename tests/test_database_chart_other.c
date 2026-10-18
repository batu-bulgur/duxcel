#include "data/database.h"
#include "platform/platform.h"
#include "transforms/chart_other.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL database chart Other:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static void clear_error(char **error) { CHECK(*error && **error); free(*error); *error=NULL; }
static Analysis source(const char *query) { return (Analysis){.kind=SOURCE_DEMO,.query=dx_strdup(query)}; }
static void assign(PivotDefinition *d,PivotRole role,const char *name,Aggregate aggregate) {
    char *error=NULL; CHECK(pivot_assign(d,role,name,aggregate,&error)); CHECK(!error);
}
static void fetch(Database *db,Analysis *a,PivotDefinition *d,uint64_t offset,int64_t total,bool totals,Page *p) {
    char *error=NULL;
    if (!database_pivot_with_other(db,a,d,offset,total,totals,0,p,&error)) { fprintf(stderr,"fetch: %s\n",error); exit(1); }
    CHECK(!error && !p->count_pending && p->bytes<=DX_PAGE_OTHER_BYTES);
}
static void number(const Page *p,size_t column,double expected) {
    CHECK(p && p->row_count==1 && column<p->column_count && !p->cells[column].is_null);
    CHECK(fabs(p->cells[column].number-expected)<1e-10);
}
static void text(const Page *p,size_t column,const char *expected) {
    CHECK(p && column<p->column_count);
    CHECK(expected ? !p->cells[column].is_null && p->cells[column].text && !strcmp(p->cells[column].text,expected) : p->cells[column].is_null && !p->cells[column].text);
}
static void same_body(const Page *a,const Page *b) {
    CHECK(a->column_count==b->column_count && a->row_count==b->row_count && a->offset==b->offset && a->total==b->total);
    for (size_t c=0;c<a->column_count;c++) CHECK(a->columns[c].type==b->columns[c].type && !strcmp(a->columns[c].name,b->columns[c].name));
    for (size_t i=0;i<a->row_count*a->column_count;i++) CHECK(a->cells[i].is_null==b->cells[i].is_null &&
        (a->cells[i].is_null || !strcmp(a->cells[i].text,b->cells[i].text)));
}
static const char *weighted="SELECT * FROM (VALUES ('A',100,100,'a'),('B',90,100,'b'),('C',80,100,'c'),('D',2,0,'x'),('E',1,10,'x'),('E',1,10,'y'),('E',1,10,'z')) t(r,rank,v,customer)";
static void weighted_definition(PivotDefinition *d) {
    assign(d,PIVOT_ROWS,"r",AGG_SUM); assign(d,PIVOT_VALUES,"rank",AGG_SUM);
    assign(d,PIVOT_VALUES,"v",AGG_AVG); assign(d,PIVOT_VALUES,"customer",AGG_DISTINCT);
    assign(d,PIVOT_VALUES,"v",AGG_COUNT); assign(d,PIVOT_VALUES,"v",AGG_MIN); assign(d,PIVOT_VALUES,"v",AGG_MAX);
}
static void test_weighted(Database *db) {
    Analysis a=source(weighted); PivotDefinition d={0}; weighted_definition(&d);
    Page old={0},p={0}; char *error=NULL;
    CHECK(database_pivot(db,&a,&d,0,-1,&old,&error)); CHECK(!old.other && old.other_status==PAGE_OTHER_NONE && !db->pivot_other_keys);
    uint64_t queries=db->query_count; fetch(db,&a,&d,0,5,true,&p);
    CHECK(db->query_count==queries+6); /* UNKNOWN replan, page, three totals executions, Other. */
    same_body(&old,&p); CHECK(p.other_status==PAGE_OTHER_READY && p.other && p.other->variant_count==1 && p.totals);
    const Page *values=page_other_values(&p,1); CHECK(values==page_other_values(&p,2));
    number(values,0,5); number(values,1,7.5); number(values,2,3); number(values,3,4); number(values,4,0); number(values,5,10);
    CHECK(p.cells[3*p.column_count+2].number==0 && p.cells[4*p.column_count+2].number==10);
    CHECK((p.cells[3*p.column_count+2].number+p.cells[4*p.column_count+2].number)/2==5);
    CHECK(p.cells[3*p.column_count+3].number+p.cells[4*p.column_count+3].number==4);
    queries=db->query_count; fetch(db,&a,&d,0,5,true,&p); CHECK(db->query_count==queries+4); /* warm plan/totals */
    CHECK(p.other_status==PAGE_OTHER_READY && page_other_values(&p,5));
    Page borrowed=p; borrowed.row_count=3; CHECK(!page_other_values(&borrowed,1)); borrowed=p; borrowed.offset++; CHECK(!page_other_values(&borrowed,1));
    borrowed=p; borrowed.total++; CHECK(!page_other_values(&borrowed,1)); borrowed=p; borrowed.column_count--; CHECK(!page_other_values(&borrowed,1));
    Page moved=p; p=(Page){0}; page_free(&p); CHECK(page_other_values(&moved,1)); page_free(&moved); page_free(&moved);
    CHECK(!moved.other && moved.other_status==PAGE_OTHER_NONE);
    /* A legacy opt-out call has no Other work even with AVG/distinct assigned. */
    page_free(&old); queries=db->query_count; CHECK(database_pivot(db,&a,&d,0,5,&old,&error)); CHECK(db->query_count==queries+1 && !old.other && !old.other_status);
    CHECK(database_refresh(db,&error) && !db->pivot_key && !db->pivot_other_keys);
    fetch(db,&a,&d,0,-1,false,&p); CHECK(p.other_status==PAGE_OTHER_READY);
    page_free(&old); page_free(&p); pivot_free(&d); analysis_free(&a); free(error);
}
static void test_no_sum(Database *db) {
    Analysis a=source(weighted); PivotDefinition d={0}; Page p={0};
    assign(&d,PIVOT_ROWS,"r",AGG_SUM); assign(&d,PIVOT_VALUES,"v",AGG_AVG); assign(&d,PIVOT_VALUES,"customer",AGG_DISTINCT);
    fetch(db,&a,&d,0,-1,false,&p); CHECK(p.other_status==PAGE_OTHER_READY && p.other->variant_count==2);
    CHECK(p.other->rank_measure[0]==0 && p.other->rank_measure[1]==1 && p.other->measure_variant[0]!=p.other->measure_variant[1]);
    number(page_other_values(&p,0),0,7.5); number(page_other_values(&p,0),1,3);
    number(page_other_values(&p,1),0,50); number(page_other_values(&p,1),1,2);
    CHECK(p.other->top_rows[0][0]==0 && p.other->top_rows[1][0]==4);
    page_free(&p); pivot_free(&d); analysis_free(&a);
}
static void test_types_and_nulls(Database *db) {
    Analysis a=source("SELECT r,c,(10-i)::DECIMAL(38,4) v,(10-i)::BIGINT rank,CASE i WHEN 3 THEN NULL WHEN 4 THEN '' WHEN 5 THEN 'NULL' ELSE 'x' END customer FROM (VALUES (0,'A'),(1,'B'),(2,'C'),(3,NULL),(4,''),(5,'NULL')) t(i,r) CROSS JOIN (VALUES(NULL::VARCHAR),(''),('NULL'),('O''İ')) q(c)");
    PivotDefinition d={0}; Page p={0};
    assign(&d,PIVOT_ROWS,"r",AGG_SUM); assign(&d,PIVOT_COLUMNS,"c",AGG_SUM);
    assign(&d,PIVOT_VALUES,"rank",AGG_SUM); assign(&d,PIVOT_VALUES,"v",AGG_AVG); assign(&d,PIVOT_VALUES,"customer",AGG_DISTINCT); assign(&d,PIVOT_VALUES,"v",AGG_SUM);
    fetch(db,&a,&d,0,-1,true,&p); CHECK(p.other_status==PAGE_OTHER_READY && p.other->value_columns==16);
    const Page *values=page_other_values(&p,1); bool missing=false,empty=false,literal=false,quoted=false;
    for (size_t c=0;c<4;c++) {
        const char *category=p.columns[1+c*4].categories[0]; missing|=!category; empty|=category && !*category;
        literal|=category && !strcmp(category,"NULL"); quoted|=category && !strcmp(category,"O'İ");
        number(values,c*4,18); number(values,c*4+1,6); number(values,c*4+2,2); text(values,c*4+3,"18.0000");
        CHECK(values->columns[c*4+3].type==DUCKDB_TYPE_DECIMAL && values->columns[c*4+3].number_type.precision==38 && values->columns[c*4+3].number_type.scale==4);
    }
    CHECK(missing && empty && literal && quoted); page_free(&p); pivot_free(&d); analysis_free(&a);
    a=source("SELECT (9007199254740993+i)::BIGINT r,make_date((2025+i)::INTEGER,1,1) occurred, (10-i)::DECIMAL(38,4) v FROM range(5) t(i)");
    assign(&d,PIVOT_ROWS,"r",AGG_SUM); assign(&d,PIVOT_COLUMNS,"occurred",AGG_SUM); d.fields[PIVOT_COLUMNS][0].grain=PIVOT_DATE_YEAR;
    assign(&d,PIVOT_VALUES,"v",AGG_AVG); assign(&d,PIVOT_VALUES,"v",AGG_SUM);
    fetch(db,&a,&d,0,-1,false,&p); CHECK(p.other_status==PAGE_OTHER_READY && !strcmp(p.cells[0].text,"9007199254740993"));
    values=page_other_values(&p,0); CHECK(values->column_count==10);
    bool null_seen=false,exact_seen=false;
    for (size_t c=0;c<10;c+=2) { null_seen|=values->cells[c].is_null; exact_seen|=!values->cells[c+1].is_null; }
    CHECK(null_seen && exact_seen); page_free(&p); pivot_free(&d); analysis_free(&a);
}
static void test_exact_and_offsets(Database *db) {
    Analysis a=source("SELECT 'r'||i::VARCHAR r,(10-i)::INTEGER rank,1234567890123456789012345678901234567::DECIMAL(38,0) v FROM range(5) t(i)");
    PivotDefinition d={0}; Page p={0}; assign(&d,PIVOT_ROWS,"r",AGG_SUM); assign(&d,PIVOT_VALUES,"rank",AGG_SUM);
    assign(&d,PIVOT_VALUES,"v",AGG_SUM); assign(&d,PIVOT_VALUES,"v",AGG_AVG);
    fetch(db,&a,&d,0,-1,false,&p); CHECK(p.other_status==PAGE_OTHER_READY);
    const Page *values=page_other_values(&p,2); text(values,1,"2469135780246913578024691357802469134");
    CHECK(values->columns[1].type==DUCKDB_TYPE_DECIMAL && values->columns[1].number_type.precision==38);
    page_free(&p); pivot_free(&d); analysis_free(&a);
    a=source("SELECT 'r'||lpad(i::VARCHAR,3,'0') r,1000-i rank,i::INTEGER v,(i%7)::INTEGER customer FROM range(600) t(i)");
    assign(&d,PIVOT_ROWS,"r",AGG_SUM); assign(&d,PIVOT_VALUES,"rank",AGG_SUM); assign(&d,PIVOT_VALUES,"v",AGG_AVG); assign(&d,PIVOT_VALUES,"customer",AGG_DISTINCT);
    fetch(db,&a,&d,256,-1,true,&p); CHECK(p.offset==256 && p.row_count==256 && p.total==600 && p.other_status==PAGE_OTHER_READY);
    values=page_other_values(&p,1); number(values,1,385); number(values,2,7);
    CHECK(p.other->top_rows[0][0]==0 && p.other->top_rows[0][2]==2);
    Page prefix=p; prefix.row_count=12; prefix.total=12; prefix.offset=0; CHECK(!page_other_values(&prefix,1));
    page_free(&p); pivot_free(&d); analysis_free(&a);
    a=source("SELECT 'r'||i::VARCHAR r,i::DOUBLE a,(9-i)::DOUBLE b,(i*2)::DOUBLE c,(i*3)::DOUBLE d,(i*4)::DOUBLE e,(i*5)::DOUBLE f,(i*6)::DOUBLE g,(i*7)::DOUBLE h FROM range(5) t(i)");
    assign(&d,PIVOT_ROWS,"r",AGG_SUM); const char *names[]={"a","b","c","d","e","f","g","h"};
    for (size_t m=0;m<8;m++) assign(&d,PIVOT_VALUES,names[m],AGG_AVG);
    fetch(db,&a,&d,0,-1,false,&p); CHECK(p.other_status==PAGE_OTHER_READY && p.other->variant_count==8 && p.other->value_columns==8);
    for (size_t m=0;m<8;m++) CHECK(page_other_values(&p,m) && p.other->rank_measure[p.other->measure_variant[m]]==m);
    number(page_other_values(&p,0),0,.5); number(page_other_values(&p,1),0,3.5);
    page_free(&p); pivot_free(&d); analysis_free(&a);
}
static void test_nul_and_unsupported(Database *db) {
    Analysis a=source("SELECT CASE i WHEN 4 THEN 'a'||chr(0)||'b' ELSE 'r'||i::VARCHAR END r,i::INTEGER v FROM range(5) t(i)");
    PivotDefinition d={0}; Page p={0},old={0}; char *error=NULL;
    assign(&d,PIVOT_ROWS,"r",AGG_SUM); assign(&d,PIVOT_VALUES,"v",AGG_AVG);
    CHECK(database_pivot(db,&a,&d,0,-1,&old,&error)); uint64_t before=db->query_count;
    fetch(db,&a,&d,0,5,false,&p); CHECK(p.other_status==PAGE_OTHER_UNSAFE_KEY && !p.other && db->query_count==before+2); same_body(&old,&p);
    page_free(&p); page_free(&old); pivot_free(&d); analysis_free(&a);
    a=source("SELECT 'r'||i::VARCHAR r,CASE j WHEN 0 THEN 'a'||chr(0)||'b' ELSE 'q' END c,i::INTEGER v FROM range(5) t(i) CROSS JOIN range(2) u(j)");
    assign(&d,PIVOT_ROWS,"r",AGG_SUM); assign(&d,PIVOT_COLUMNS,"c",AGG_SUM); assign(&d,PIVOT_VALUES,"v",AGG_AVG);
    CHECK(database_pivot(db,&a,&d,0,-1,&old,&error)); before=db->query_count;
    fetch(db,&a,&d,0,5,false,&p); CHECK(p.other_status==PAGE_OTHER_UNSAFE_KEY && !p.other && db->query_count==before+3); same_body(&old,&p);
    CHECK(db->pivot_other_keys!=0); CHECK(database_refresh(db,&error) && !db->pivot_other_keys);
    page_free(&old); pivot_free(&d); analysis_free(&a);
    a=source("SELECT [i] r,i::INTEGER v FROM range(5) t(i)"); assign(&d,PIVOT_ROWS,"r",AGG_SUM); assign(&d,PIVOT_VALUES,"v",AGG_AVG);
    Page preserved=p; CHECK(!database_pivot_with_other(db,&a,&d,0,-1,false,0,&p,&error));
    CHECK(!memcmp(&p,&preserved,sizeof(p))); clear_error(&error);
    page_free(&p); pivot_free(&d); analysis_free(&a); free(error);
}
static void test_limits(Database *db) {
    Analysis a=source("SELECT repeat(chr((65+i)::INTEGER),900000) r,i::INTEGER v FROM range(4) t(i)");
    PivotDefinition d={0}; Page p={0}; assign(&d,PIVOT_ROWS,"r",AGG_SUM); assign(&d,PIVOT_VALUES,"v",AGG_AVG);
    uint64_t before=db->query_count; fetch(db,&a,&d,0,-1,false,&p);
    CHECK(p.other_status==PAGE_OTHER_LIMIT && !p.other && p.row_count==4 && db->query_count==before+3);
    page_free(&p); pivot_free(&d); analysis_free(&a);
    a=source("SELECT 'r'||i::VARCHAR r,i::INTEGER v FROM range(3) t(i)"); assign(&d,PIVOT_ROWS,"r",AGG_SUM); assign(&d,PIVOT_VALUES,"v",AGG_AVG);
    fetch(db,&a,&d,0,-1,false,&p); CHECK(p.other_status==PAGE_OTHER_NOT_NEEDED && !p.other);
    page_free(&p); pivot_free(&d); analysis_free(&a);
    a=source("SELECT 'r'||i::VARCHAR r,i::INTEGER v FROM range(5) t(i)"); assign(&d,PIVOT_ROWS,"r",AGG_SUM); assign(&d,PIVOT_VALUES,"v",AGG_SUM);
    fetch(db,&a,&d,0,-1,false,&p); CHECK(p.other_status==PAGE_OTHER_NOT_NEEDED && !p.other);
    page_free(&p); pivot_free(&d); analysis_free(&a);
}
static unsigned releases[2];
static void release_a(void *text) { releases[0]++; free(text); }
static void release_b(void *text) { releases[1]++; free(text); }
static Page synthetic(size_t rows,size_t columns,const char *prefix) {
    Page p={.row_count=rows,.column_count=columns,.total=rows,.bytes=columns*sizeof(Column)+rows*columns*sizeof(Cell)};
    p.columns=dx_alloc(columns*sizeof(Column)); p.cells=dx_alloc(rows*columns*sizeof(Cell));
    for (size_t c=0;c<columns;c++) {
        p.columns[c]=(Column){.name=dx_printf("%s%zu",prefix,c),.type_name=dx_strdup("DOUBLE"),.type=DUCKDB_TYPE_DOUBLE,.number_type={NUM_FLOAT,0,0},.numeric=true};
        p.bytes+=strlen(p.columns[c].name)+8;
    }
    for (size_t i=0;i<rows*columns;i++) { p.cells[i].text=dx_strdup("1"); p.cells[i].number=1; p.bytes+=2; }
    return p;
}
static void test_owned_adoption(void) {
    Page p=synthetic(5,3,"c"); p.sql=dx_strdup("real captured relation"); p.bytes+=strlen(p.sql)+1;
    PageOther *other=dx_alloc(sizeof(*other)); other->keys=1; other->measures=2; other->value_columns=2; other->top_count=3; other->variant_count=2;
    for (size_t m=0;m<DX_PIVOT_FIELDS;m++) other->measure_variant[m]=SIZE_MAX;
    for (size_t v=0;v<2;v++) {
        other->variants[v]=synthetic(1,2,"__dx_other_"); other->variants[v].release_cell_text=v ? release_b : release_a;
        other->rank_measure[v]=v; other->measure_variant[v]=v;
        for (size_t k=0;k<3;k++) other->top_rows[v][k]=k;
    }
    char *error=NULL; Page before=p; PageOther *identity=other;
    other->rank_measure[1]=0; CHECK(!page_other_adopt(&p,&other,&error)); CHECK(!memcmp(&p,&before,sizeof(p)) && other==identity); clear_error(&error); other->rank_measure[1]=1;
    Column *independent=other->variants[0].columns; other->variants[0].columns=p.columns;
    CHECK(!page_other_adopt(&p,&other,&error)); CHECK(other==identity && !memcmp(&p,&before,sizeof(p))); clear_error(&error); other->variants[0].columns=independent;
    other->variants[7]=synthetic(1,2,"__dx_other_"); CHECK(!page_other_adopt(&p,&other,&error)); clear_error(&error); page_free(&other->variants[7]);
    other->variants[1].columns[0].type=DUCKDB_TYPE_INTEGER; CHECK(!page_other_adopt(&p,&other,&error)); clear_error(&error); other->variants[1].columns[0].type=DUCKDB_TYPE_DOUBLE;
    p.bytes=DX_PAGE_OTHER_BYTES; CHECK(!page_other_adopt(&p,&other,&error)); clear_error(&error); p.bytes=before.bytes;
    CHECK(page_other_adopt(&p,&other,&error) && !other && p.other_status==PAGE_OTHER_READY);
    CHECK(page_other_values(&p,0) && page_other_values(&p,1) && !page_other_values(&p,2));
    Page prefix=p; prefix.row_count--; CHECK(!page_other_values(&prefix,0)); prefix=p; prefix.sql=NULL; CHECK(!page_other_values(&prefix,0));
    Page moved=p; p=(Page){0}; page_free(&p); CHECK(!releases[0] && !releases[1]); page_free(&moved); page_free(&moved);
    CHECK(releases[0]==2 && releases[1]==2 && !moved.cells && !moved.other);
}
typedef struct { Database *db; uint64_t threshold; bool fired,cancel; const char *path; } Checkpoint;
static bool checkpoint(void *pointer) {
    Checkpoint *p=pointer;
    if (!p->fired && p->db->query_count>=p->threshold) {
        p->fired=true;
        if (p->path) { FILE *f=fopen(p->path,"a"); CHECK(f); CHECK(fputs("Z,99\n",f)>=0); CHECK(!fclose(f)); }
    }
    return p->fired && p->cancel;
}
static void test_atomic_and_guard(Database *db,const char *directory) {
    Analysis a=source(weighted); PivotDefinition d={0}; weighted_definition(&d); pivot_remove(&d,PIVOT_VALUES,0); Page p={0}; fetch(db,&a,&d,0,-1,false,&p);
    Page before=p; char *error=NULL; Checkpoint cp={.db=db,.threshold=db->query_count+2,.cancel=true};
    db->cancelled=checkpoint; db->cancel_context=&cp;
    CHECK(!database_pivot_with_other(db,&a,&d,0,5,false,0,&p,&error)); CHECK(cp.fired && !memcmp(&p,&before,sizeof(p))); clear_error(&error);
    db->cancelled=NULL; db->cancel_context=NULL; CHECK(page_other_values(&p,1));
    Analysis overflow=source("SELECT 'r'||i::VARCHAR r,90000000000000000000000000000000000000::DECIMAL(38,0) v FROM range(5) t(i)");
    PivotDefinition overflow_definition={0}; assign(&overflow_definition,PIVOT_ROWS,"r",AGG_SUM);
    assign(&overflow_definition,PIVOT_VALUES,"v",AGG_SUM); assign(&overflow_definition,PIVOT_VALUES,"v",AGG_AVG);
    uint64_t queries=db->query_count;
    CHECK(!database_pivot_with_other(db,&overflow,&overflow_definition,0,-1,false,0,&p,&error));
    CHECK(!memcmp(&p,&before,sizeof(p)) && db->query_count==queries+4 && error && (strstr(error,"Overflow") || strstr(error,"overflow"))); clear_error(&error);
    pivot_free(&overflow_definition); analysis_free(&overflow);
    free(a.query); a.query=dx_strdup("SELECT missing FROM no_such_relation");
    CHECK(!database_pivot_with_other(db,&a,&d,0,-1,false,0,&p,&error)); CHECK(!memcmp(&p,&before,sizeof(p))); clear_error(&error);
    analysis_free(&a); pivot_free(&d); page_free(&p);
    char *path=dx_printf("%s/source.csv",directory); FILE *f=fopen(path,"w"); CHECK(f); CHECK(fputs("r,v\nA,100\nB,90\nC,80\nD,0\nE,10\n",f)>=0); CHECK(!fclose(f));
    a=(Analysis){.kind=SOURCE_CSV,.path=dx_strdup(path)}; assign(&d,PIVOT_ROWS,"r",AGG_SUM); assign(&d,PIVOT_VALUES,"v",AGG_AVG);
    fetch(db,&a,&d,0,-1,false,&p); before=p; uint64_t stamp=dx_file_stamp(path); CHECK(stamp);
    CHECK(!database_pivot_with_other(db,&a,&d,0,5,false,stamp^1,&p,&error)); CHECK(!memcmp(&p,&before,sizeof(p))); clear_error(&error);
    cp=(Checkpoint){.db=db,.threshold=db->query_count+2,.path=path}; db->cancelled=checkpoint; db->cancel_context=&cp;
    CHECK(!database_pivot_with_other(db,&a,&d,0,5,false,stamp,&p,&error)); CHECK(cp.fired && !memcmp(&p,&before,sizeof(p))); clear_error(&error);
    db->cancelled=NULL; db->cancel_context=NULL;
    fetch(db,&a,&d,0,-1,false,&p); CHECK(p.total==6 && p.other_status==PAGE_OTHER_READY);
    page_free(&p); pivot_free(&d); analysis_free(&a); CHECK(!unlink(path)); free(path);
}
int main(void) {
    char directory[]="/tmp/duxcel-database-other-XXXXXX"; CHECK(mkdtemp(directory));
    Database db={0}; char *error=NULL; CHECK(database_open(&db,&error)); CHECK(!error);
    test_owned_adoption(); test_weighted(&db); test_no_sum(&db); test_types_and_nulls(&db);
    test_exact_and_offsets(&db); test_nul_and_unsupported(&db); test_limits(&db); test_atomic_and_guard(&db,directory);
    database_close(&db); free(error); CHECK(!rmdir(directory));
    printf("PASS: database chart Other %u checks; real native weighted/distinct/typed/key-proof/ownership gates\n",checks); return 0;
}

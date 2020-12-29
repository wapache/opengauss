--begin: these test are related to explain output change about partition table.
--      major change is as below
	--1.
	--Selected Partitions:  1  2  6  7  8  9
	--                          \|/
	--Selected Partitions:  1..2,6..9
	--2.
	--Selected Partitions:  1  3  5  7  9
	--                          \|/
	--Selected Partitions:  1,3,5,7,9
CREATE schema FVT_COMPRESS;
set search_path to FVT_COMPRESS;


create table test_explain_format_on_part_table (id int) 
partition by range(id) 
(
partition p1 values less than (10), 
partition p2 values less than (20), 
partition p3 values less than (30), 
partition p4 values less than (40),
partition p5 values less than (50),
partition p6 values less than (60),
partition p7 values less than (70),
partition p8 values less than (80),
partition p9 values less than (90)
)ENABLE ROW MOVEMENT;
-- two continous segments, text formast
explain (verbose on, costs off) 
	select * from test_explain_format_on_part_table where id <15 or id >51;
-- no continous segment, text formast
explain (verbose on, costs off) 
	select * from test_explain_format_on_part_table where id =5 or id =25 or id=45 or id = 65 or id = 85;
-- two continous segments, non-text formast
explain (verbose on, costs off, FORMAT JSON) 
	select * from test_explain_format_on_part_table where id <15 or id >51;
-- no continous segment, non-text formast
explain (verbose on, costs off, FORMAT JSON) 
	select * from test_explain_format_on_part_table where id =5 or id =25 or id=45 or id = 65 or id = 85;

drop table test_explain_format_on_part_table;
--end: these test are related to explain output change about partition table.
CREATE schema FVT_COMPRESS;
--I1. 创建源表
CREATE TABLE COMPRESS_TABLE_077_1(
        c_int_1 BIGINT,
        c_char_1 CHAR(50),
        c_int_2 BIGINT,
        c_dec_1 DECIMAL(10,4),
        c_char_2 CHAR(50),
        c_tsw_1 TIMESTAMP,
        c_text_1 text,
        c_date_1 DATE, 
        c_tsw_2 TIMESTAMP,
        c_date_2 DATE,
        c_text_2 text,
        c_nvarchar_1 NVARCHAR2(100),
        c_nvarchar_2 NVARCHAR2(100),
        c_dec_2 DECIMAL(10,4));

--I2. 创建目标表，与源表的格式完全相同
CREATE TABLE COMPRESS_TABLE_077_2(
c_int_1 BIGINT,
c_char_1 CHAR(50),
c_int_2 BIGINT,
c_dec_1 DECIMAL(10,4),
c_char_2 CHAR(50),
c_tsw_1 TIMESTAMP,
c_text_1 text,
c_date_1 DATE, 
c_tsw_2 TIMESTAMP,
c_date_2 DATE,
c_text_2 text,
c_nvarchar_1 NVARCHAR2(100),
c_nvarchar_2 NVARCHAR2(100),
c_dec_2 DECIMAL(10,4)) PARTITION BY RANGE(c_date_1)
( 
    PARTITION COMPRESS_TABLE_INTEVAL_077_2_1  VALUES LESS THAN('2012-8-23')
)ENABLE ROW MOVEMENT;

--I3. 给目标表创建索引
CREATE INDEX INDEX_COMPRESS_077_1 ON COMPRESS_TABLE_077_2 USING BTREE(C_DATE_1) LOCAL;
--I4. 查询,使用索引、子句
(SELECT COUNT(*) FROM COMPRESS_TABLE_077_1 WHERE C_DATE_1>='2012-09-01' AND C_DATE_1<'2012-9-20') 
MINUS ALL (SELECT COUNT(*) FROM COMPRESS_TABLE_077_2 WHERE C_DATE_1>='2012-09-01' AND C_DATE_1<'2012-9-20');

create table hw_partition_select_parttable (
   c1 int,
   c2 int,
   c3 text)
partition by range(c1)
(partition hw_partition_select_parttable_p1 values less than (50),
 partition hw_partition_select_parttable_p2 values less than (150),
 partition hw_partition_select_parttable_p3 values less than (400));
 
 insert into hw_partition_select_parttable values (-100,40,'abc');
 insert into hw_partition_select_parttable(c1,c2) values (100,20);
 insert into hw_partition_select_parttable values(300,200);
 
select * from hw_partition_select_parttable order by 1, 2, 3;

select c1 from hw_partition_select_parttable order by 1;

select c1,c2 from hw_partition_select_parttable order by 1, 2;

select c2 from hw_partition_select_parttable order by 1;

select c1,c2,c3 from hw_partition_select_parttable order by 1, 2, 3;

select c1 from hw_partition_select_parttable where c1>50 and c1<300 order by 1;

select * from hw_partition_select_parttable where c2>100 order by 1, 2, 3;

create table t_select_datatype_int32(c1 int,c2 int,c3 int,c4 text)
partition by range(c1)
(partition t_select_datatype_int32_p1 values less than(100),
 partition t_select_datatype_int32_p2 values less than(200),
 partition t_select_datatype_int32_p3 values less than(300),
 partition t_select_datatype_int32_p4 values less than(500));
 
insert into t_select_datatype_int32 values(-100,20,20,'a'),
               		 (100,300,300,'bb'),
			         (150,75,500,NULL),
			         (200,500,50,'ccc'),
			         (250,50,50,NULL),
			         (300,700,125,''),
			         (450,35,150,'dddd');

--partition select for int32
--success
select * from t_select_datatype_int32 where t_select_datatype_int32.c1=50 order by 1, 2, 3, 4;

--success
select * from t_select_datatype_int32 where t_select_datatype_int32.c1=100 order by 1, 2, 3, 4;

--success
select * from t_select_datatype_int32 where t_select_datatype_int32.c1=250 order by 1, 2, 3, 4;

--success
select * from t_select_datatype_int32 where t_select_datatype_int32.c1=500 order by 1, 2, 3, 4;

--success
select * from t_select_datatype_int32 where t_select_datatype_int32.c1=550 order by 1, 2, 3, 4;

--success
select * from t_select_datatype_int32 where t_select_datatype_int32.c1<50 order by 1, 2, 3, 4;

--success
select * from t_select_datatype_int32 where t_select_datatype_int32.c1<=50 order by 1, 2, 3, 4;

--success
select * from t_select_datatype_int32 where t_select_datatype_int32.c1<100 order by 1, 2, 3, 4;

--success
select * from t_select_datatype_int32 where t_select_datatype_int32.c1<=100 order by 1, 2, 3, 4;

--success
select * from t_select_datatype_int32 where t_select_datatype_int32.c1<150 order by 1, 2, 3, 4;

--success
select * from t_select_datatype_int32 where t_select_datatype_int32.c1<200 order by 1, 2, 3, 4;

--success
select * from t_select_datatype_int32 where t_select_datatype_int32.c1<=200 order by 1, 2, 3, 4;

--success
select * from t_select_datatype_int32 where t_select_datatype_int32.c1<500 order by 1, 2, 3, 4;

--success
select * from t_select_datatype_int32 where t_select_datatype_int32.c1<=500 order by 1, 2, 3, 4;

--success
select * from t_select_datatype_int32 where t_select_datatype_int32.c1<700 order by 1, 2, 3, 4;

--success
select * from t_select_datatype_int32 where t_select_datatype_int32.c1<=700 order by 1, 2, 3, 4;

--success
select * from t_select_datatype_int32 where t_select_datatype_int32.c1>50 order by 1, 2, 3, 4;

--success
select * from t_select_datatype_int32 where t_select_datatype_int32.c1>=50 order by 1, 2, 3, 4;

--success
select * from t_select_datatype_int32 where t_select_datatype_int32.c1>100 order by 1, 2, 3, 4;

--success
select * from t_select_datatype_int32 where t_select_datatype_int32.c1>=100 order by 1, 2, 3, 4;

--success
select * from t_select_datatype_int32 where t_select_datatype_int32.c1>150 order by 1, 2, 3, 4;

--success
select * from t_select_datatype_int32 where t_select_datatype_int32.c1>=150 order by 1, 2, 3, 4;

--success
select * from t_select_datatype_int32 where t_select_datatype_int32.c1>200 order by 1, 2, 3, 4;

--success
select * from t_select_datatype_int32 where t_select_datatype_int32.c1>=200 order by 1, 2, 3, 4;

--success
select * from t_select_datatype_int32 where t_select_datatype_int32.c1>500 order by 1, 2, 3, 4;

--success
select * from t_select_datatype_int32 where t_select_datatype_int32.c1>=500 order by 1, 2, 3, 4;

--success
select * from t_select_datatype_int32 where t_select_datatype_int32.c1<50 AND t_select_datatype_int32.c1<250 order by 1, 2, 3, 4;

--success
select * from t_select_datatype_int32 where t_select_datatype_int32.c1<50 AND t_select_datatype_int32.c1>0 order by 1, 2, 3, 4;

--success
select * from t_select_datatype_int32 where t_select_datatype_int32.c1<50 AND t_select_datatype_int32.c1>100 order by 1, 2, 3, 4;

--success
select * from t_select_datatype_int32 where t_select_datatype_int32.c1>50 AND t_select_datatype_int32.c1>=150 order by 1, 2, 3, 4;

--success
select * from t_select_datatype_int32 where t_select_datatype_int32.c1>100 AND t_select_datatype_int32.c1>=100 order by 1, 2, 3, 4;

--success
select * from t_select_datatype_int32 where t_select_datatype_int32.c1>=100 AND t_select_datatype_int32.c1=100 order by 1, 2, 3, 4;

--success
select * from t_select_datatype_int32 where t_select_datatype_int32.c1>=100 AND t_select_datatype_int32.c1<300 order by 1, 2, 3, 4;

--success
select * from t_select_datatype_int32 where t_select_datatype_int32.c1>=100 AND t_select_datatype_int32.c1<550 order by 1, 2, 3, 4;

--success
select * from t_select_datatype_int32 where t_select_datatype_int32.c1>100 AND t_select_datatype_int32.c1<=500 AND t_select_datatype_int32.c1>=100 AND t_select_datatype_int32.c1<500 order by 1, 2, 3, 4;

--success
select * from t_select_datatype_int32 where t_select_datatype_int32.c1>250 AND t_select_datatype_int32.c1<50 order by 1, 2, 3, 4;

--success
select * from t_select_datatype_int32 where t_select_datatype_int32.c1>50 AND t_select_datatype_int32.c1>100 AND t_select_datatype_int32.c1>=100 AND t_select_datatype_int32.c1<250 AND t_select_datatype_int32.c1<=250 AND t_select_datatype_int32.c1=200 order by 1, 2, 3, 4;

--success
select * from t_select_datatype_int32 where t_select_datatype_int32.c1<50 OR t_select_datatype_int32.c1<250 order by 1, 2, 3, 4;

--success
select * from t_select_datatype_int32 where t_select_datatype_int32.c1<50 OR t_select_datatype_int32.c1>0 order by 1, 2, 3, 4;

--success
select * from t_select_datatype_int32 where t_select_datatype_int32.c1<50 OR t_select_datatype_int32.c1>100 order by 1, 2, 3, 4;

--success
select * from t_select_datatype_int32 where t_select_datatype_int32.c1>50 OR t_select_datatype_int32.c1>=150 order by 1, 2, 3, 4;

--success
select * from t_select_datatype_int32 where t_select_datatype_int32.c1>100 OR t_select_datatype_int32.c1>=100 order by 1, 2, 3, 4;

--success
select * from t_select_datatype_int32 where t_select_datatype_int32.c1>=100 OR t_select_datatype_int32.c1=100 order by 1, 2, 3, 4;

--success
select * from t_select_datatype_int32 where t_select_datatype_int32.c1>=100 OR t_select_datatype_int32.c1<200 order by 1, 2, 3, 4;

--success
select * from t_select_datatype_int32 where t_select_datatype_int32.c1>500 OR t_select_datatype_int32.c1<250 order by 1, 2, 3, 4;

--success
select * from t_select_datatype_int32 where t_select_datatype_int32.c1>100 OR t_select_datatype_int32.c1<=300 OR t_select_datatype_int32.c1>=100 OR t_select_datatype_int32.c1<300 order by 1, 2, 3, 4;

--success
select * from t_select_datatype_int32 where t_select_datatype_int32.c1>250 OR t_select_datatype_int32.c1<50 order by 1, 2, 3, 4;

--success
select * from t_select_datatype_int32 where t_select_datatype_int32.c1<170  AND ( t_select_datatype_int32.c1>600 OR t_select_datatype_int32.c1<150) order by 1, 2, 3, 4;

--success
select * from t_select_datatype_int32 where (t_select_datatype_int32.c1<170 OR t_select_datatype_int32.c1<250)  AND ( t_select_datatype_int32.c1>600 OR t_select_datatype_int32.c1<150) order by 1, 2, 3, 4;

--success
select * from t_select_datatype_int32 where t_select_datatype_int32.c1<50 OR t_select_datatype_int32.c1>250 AND t_select_datatype_int32.c1<400 order by 1, 2, 3, 4;

--success
select * from t_select_datatype_int32 where t_select_datatype_int32.c1>=-100 AND t_select_datatype_int32.c1<50 OR t_select_datatype_int32.c1>300 AND t_select_datatype_int32.c1<700 order by 1, 2, 3, 4; 

--success
select * from t_select_datatype_int32 where t_select_datatype_int32.c1>=-100 AND t_select_datatype_int32.c1<=100 OR t_select_datatype_int32.c1>300 AND t_select_datatype_int32.c1<700 order by 1, 2, 3, 4; 

--IS NULL
--success
select * from t_select_datatype_int32 where 
	(t_select_datatype_int32.c1>500 OR t_select_datatype_int32.c1<250) AND 
	(t_select_datatype_int32.c1>300 AND t_select_datatype_int32.c1<t_select_datatype_int32.c2) AND 
	(t_select_datatype_int32.c2<t_select_datatype_int32.c3 OR t_select_datatype_int32.c2>100) OR 
	t_select_datatype_int32.c4 IS NULL
	ORDER BY 1, 2, 3, 4;

--success
select * from t_select_datatype_int32 where 
	(t_select_datatype_int32.c1>500 OR t_select_datatype_int32.c1<250) AND 
	(t_select_datatype_int32.c1>300 AND t_select_datatype_int32.c1<t_select_datatype_int32.c2) AND 
	(t_select_datatype_int32.c2<t_select_datatype_int32.c3 OR t_select_datatype_int32.c2>100) AND 
	t_select_datatype_int32.c4 IS NULL
	ORDER BY 1, 2, 3, 4;

--success
select * from t_select_datatype_int32 where 
	t_select_datatype_int32.c4 IS NULL AND 
	(t_select_datatype_int32.c1>500 OR t_select_datatype_int32.c1<250) AND 
	(t_select_datatype_int32.c1>300 AND t_select_datatype_int32.c1<t_select_datatype_int32.c2) AND 
	(t_select_datatype_int32.c2<t_select_datatype_int32.c3 OR t_select_datatype_int32.c2>100)
	ORDER BY 1, 2, 3, 4;

--success
select * from t_select_datatype_int32 where 
	t_select_datatype_int32.c4 IS NULL OR 
	(t_select_datatype_int32.c1>500 OR t_select_datatype_int32.c1<250) AND 
	(t_select_datatype_int32.c1>300 AND t_select_datatype_int32.c1<t_select_datatype_int32.c2) AND 
	(t_select_datatype_int32.c2<t_select_datatype_int32.c3 OR t_select_datatype_int32.c2>100)
	ORDER BY 1, 2, 3, 4;

--success
select * from t_select_datatype_int32 where 
	(t_select_datatype_int32.c1>500 OR t_select_datatype_int32.c1<250) AND 
	(t_select_datatype_int32.c1>300 AND t_select_datatype_int32.c4 IS NULL) AND 
	(t_select_datatype_int32.c2<t_select_datatype_int32.c3 OR t_select_datatype_int32.c2>100)
	ORDER BY 1, 2, 3, 4;

--success
select * from t_select_datatype_int32 where 
	(t_select_datatype_int32.c1>500 OR t_select_datatype_int32.c1<250) AND 
	(t_select_datatype_int32.c1>300 AND t_select_datatype_int32.c1<t_select_datatype_int32.c2) AND 
	(t_select_datatype_int32.c4 IS NULL OR t_select_datatype_int32.c2>100)
	ORDER BY 1, 2, 3, 4;

--------------------------------------------------------------------------------------------------------------------------------------------------------------------------
-- check select contarins partition

--
---- check select from range partition
--

create table hw_partition_select_ordinary_table (a int, b int);

create table test_select_range_partition (a int, b int) 
partition by range(a) 
(
	partition test_select_range_partition_p1 values less than (1), 
	partition test_select_range_partition_p2 values less than (4),
	partition test_select_range_partition_p3 values less than (7)
);

insert into test_select_range_partition values(2);

--success
select * from test_select_range_partition partition (test_select_range_partition_p1) order by 1, 2;

--success
select * from test_select_range_partition partition (test_select_range_partition_p2) order by 1, 2;

--success
select * from test_select_range_partition partition (test_select_range_partition_p3) order by 1, 2;

--success
select * from test_select_range_partition partition (test_select_range_partition_p4) order by 1, 2;

--success
select a from test_select_range_partition partition (test_select_range_partition_p2) order by 1;

--success
select a from test_select_range_partition partition for (0) order by 1;

--success
select a from test_select_range_partition partition for (1) order by 1;

--success
select a from test_select_range_partition partition for (2) order by 1;

--success
select a from test_select_range_partition partition for (5) order by 1;

--success
select a from test_select_range_partition partition for (8) order by 1;

-- fail: table is not partitioned table
select a from hw_partition_select_ordinary_table partition (test_select_range_partition_p2);

-- fail: table is not partitioned table
select a from hw_partition_select_ordinary_table partition for (2);

--
---- check select from interval partition
--

create table hw_partition_select_ordinary_table (a int, b int);

create table test_select_interval_partition (a int, b int) 
partition by range(a) 
(
	partition test_select_interval_partition_p1 values less than (1), 
	partition test_select_interval_partition_p2 values less than (4),
	partition test_select_interval_partition_p3 values less than (20)
);

insert into test_select_interval_partition values(2);
insert into test_select_interval_partition values(10);

--success
select * from test_select_interval_partition partition (test_select_interval_partition_p1) order by 1;

--success
select * from test_select_interval_partition partition (test_select_interval_partition_p2) order by 1;

--success
select a from test_select_interval_partition partition (test_select_interval_partition_p3) order by 1;

--success
select a from test_select_interval_partition partition (test_select_interval_partition_p4) order by 1;

--success
select a from test_select_interval_partition partition (test_select_interval_partition_p2) order by 1;

--success
select a from test_select_interval_partition partition for (1) order by 1;

--success
select a from test_select_interval_partition partition for (2) order by 1;

--success
select a from test_select_interval_partition partition for (5) order by 1;

--success
select a from test_select_interval_partition partition for (10) order by 1;

--success
select a from test_select_interval_partition partition for (11) order by 1;

-- fail: table is not partitioned table
select a from hw_partition_select_ordinary_table partition (test_select_interval_partition_p2) order by 1;

-- fail: table is not partitioned table
select a from hw_partition_select_ordinary_table partition for (2) order by 1;

--select for update
/*
create table test_partition_for_update (c1 int,c2 int)
partition by range(c1)
(
	partition test_partition_for_update_p1 values less than(10),
	partition test_partition_for_update_p2 values less than(40)
);

insert into test_partition_for_update values(5,15);
insert into test_partition_for_update values(15,25);
insert into test_partition_for_update values(35,35);

select * from test_partition_for_update order by 1, 2 for update;

select * from test_partition_for_update partition for (35) order by 1, 2 for update;

create table test_partition_for_update_2 (c1 int,c2 int)
partition by range(c1)
(
	partition test_partition_for_update_2_p1 values less than(100),
	partition test_partition_for_update_2_p2 values less than(200)
);

insert into test_partition_for_update_2 values(50,50);
insert into test_partition_for_update_2 values(150,150);

select c1 from test_partition_for_update_2 order by 1 for update;
select c1 from test_partition_for_update_2 partition (test_partition_for_update_2_p1) order by 1 for update;

select t1.c1,t2.c2 from test_partition_for_update t1,test_partition_for_update_2 t2 where t1.c1<t2.c2 order by 1, 2 for update of t1,t2;

select t1.c1,t2.c2 from test_partition_for_update t1,test_partition_for_update_2 t2 where t1.c1<t2.c2 order by 1, 2 for update of t1;

--clean
drop table test_partition_for_update;
drop table test_partition_for_update_2;
*/


--
--
CREATE TABLE hw_partition_select_test(C_INT INTEGER)
 partition by range (C_INT)
( 
     partition hw_partition_select_test_part_1 values less than ( 400),
     partition hw_partition_select_test_part_2 values less than ( 700),
     partition hw_partition_select_test_part_3 values less than ( 1000)
);
insert  into hw_partition_select_test values(111);
insert  into hw_partition_select_test values(555);
insert  into hw_partition_select_test values(888);

select a.*  from hw_partition_select_test partition(hw_partition_select_test_part_1) a;

--
--
CREATE TABLE select_partition_table_000_1(
 C_CHAR_1 CHAR(1),
 C_CHAR_2 CHAR(10),
 C_CHAR_3 CHAR(102400),
 C_VARCHAR_1 VARCHAR(1),
 C_VARCHAR_2 VARCHAR(10),
 C_VARCHAR_3 VARCHAR(1024),
 C_INT INTEGER,
 C_BIGINT BIGINT,
 C_SMALLINT SMALLINT,
 C_FLOAT FLOAT,
 C_NUMERIC numeric(10,5),
 C_DP double precision,
 C_DATE DATE,
 C_TS_WITHOUT TIMESTAMP WITHOUT TIME ZONE,
 C_TS_WITH TIMESTAMP WITH TIME ZONE ) 
 partition by range (C_CHAR_3)
( 
     partition select_partition_000_1_1 values less than ('D'),
     partition select_partition_000_1_2 values less than ('G'),
     partition select_partition_000_1_3 values less than ('K')
);

INSERT INTO select_partition_table_000_1 VALUES('A','ABC','ABCDEFG','a','abc','abcdefg',111,111111,11,1.1,1.11,1.111,'2000-01-01','2000-01-01 01:01:01','2000-01-01 01:01:01+01');
INSERT INTO select_partition_table_000_1 VALUES('B','BCD','BCDEFGH','b','bcd','bcdefgh',222,222222,22,2.2,2.22,2.222,'2000-02-02','2000-02-02 02:02:02','2000-02-02 02:02:02+02');
INSERT INTO select_partition_table_000_1 VALUES('C','CDE','CDEFGHI','c','cde','cdefghi',333,333333,33,3.3,3.33,3.333,'2000-03-03','2000-03-03 03:03:03','2000-03-03 03:03:03+03');
INSERT INTO select_partition_table_000_1 VALUES('D','DEF','DEFGHIJ','d','def','defghij',444,444444,44,4.4,4.44,4.444,'2000-04-04','2000-04-04 04:04:04','2000-04-04 04:04:04+04');
INSERT INTO select_partition_table_000_1 VALUES('E','EFG','EFGHIJK','e','efg','efghijk',555,555555,55,5.5,5.55,5.555,'2000-05-05','2000-05-05 05:05:05','2000-05-05 05:05:05+05');
INSERT INTO select_partition_table_000_1 VALUES('F','FGH','FGHIJKL','f','fgh','fghijkl',666,666666,66,6.6,6.66,6.666,'2000-06-06','2000-06-06 06:06:06','2000-06-06 06:06:06+06');
INSERT INTO select_partition_table_000_1 VALUES('G','GHI','GHIJKLM','g','ghi','ghijklm',699,777777,77,7.7,7.77,7.777,'2000-07-07','2000-07-07 07:07:07','2000-07-07 07:07:07+07');
INSERT INTO select_partition_table_000_1 VALUES('G','GHI','GHIJKLM','g','ghi','ghijklm',777,777777,77,7.7,7.77,7.777,'2000-07-07','2000-07-07 07:07:07','2000-07-07 07:07:07+07');
INSERT INTO select_partition_table_000_1 VALUES('H','HIJ','HIJKLMN','h','hij','hijklmn',888,888888,88,8.8,8.88,8.888,'2000-08-08','2000-08-08 08:08:08','2000-08-08 08:08:08+08');
INSERT INTO select_partition_table_000_1 VALUES('I','IJK','IJKLMNO','i','ijk','ijklmno',999,999999,99,9.9,9.99,9.999,'2000-09-09','2000-09-09 09:09:09','2000-09-09 09:09:09+09');
INSERT INTO select_partition_table_000_1 VALUES('H','HIJ','HIJKLMN','h','hij','hijklmn',300,888888,88,8.8,8.88,8.888,'2000-08-08','2000-08-08 08:08:08','2000-08-08 08:08:08+08');
INSERT INTO select_partition_table_000_1 VALUES('H','HIJ','HIJKLMN','h','hij','hijklmn',600,888888,88,8.8,8.88,8.888,'2000-08-08','2000-08-08 08:08:08','2000-08-08 08:08:08+08');

select * from select_partition_table_000_1 partition for ('GHIJKLM') order by 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15;

--
--
create table hw_partition_select_test_tbl(a int)PARTITION BY RANGE(a)  (partition hw_partition_select_test_tbl_part_1 values less than (5), partition hw_partition_select_test_tbl_part_2 values less than (10));

insert into hw_partition_select_test_tbl values(3);
insert into hw_partition_select_test_tbl values(8);

with temp1 as (select * from hw_partition_select_test_tbl) select * from temp1 partition(hw_partition_select_test_tbl_part_1);

--
---- check  partition and view
--
create table test_partition_view(a int)PARTITION BY RANGE(a)  (partition test_partition_view_part_1 values less than (5), partition test_partition_view_part_2 values less than (10));

insert into test_partition_view values(3);
insert into test_partition_view values(8);

create view view_temp (a) as select * from test_partition_view;

select * from view_temp partition (test_partition_view_part_1);

create table  range_partitioned_table (a int)
partition by range(a)
(
	partition range_partitioned_table_p1 values less than (1),
	partition range_partitioned_table_p2 values less than (4),
	partition range_partitioned_table_p3 values less than (7)
);

insert into range_partitioned_table values (1);
insert into range_partitioned_table values (2);
insert into range_partitioned_table values (5);
insert into range_partitioned_table values (6);

with tmp1 as (select a from range_partitioned_table partition for (2)) select a from tmp1 order by 1;

--
---- select union select
--
create table UNION_TABLE_043_1(C_CHAR CHAR(103500),  C_VARCHAR VARCHAR(1035),  C_INT INTEGER not null,  C_DP double precision, C_TS_WITHOUT TIMESTAMP WITHOUT TIME ZONE)
partition by range (C_TS_WITHOUT)
( 
     partition UNION_TABLE_043_1_1  values less than ('2000-05-01'),
     partition UNION_TABLE_043_1_2  values less than ('2000-10-01')
);
insert into UNION_TABLE_043_1 values('ABCDEFG','abcdefg',111,1.111,'2000-01-01 01:01:01');
insert into UNION_TABLE_043_1 values('BCDEFGH','bcdefgh',222,2.222,'2000-02-02 02:02:02');
insert into UNION_TABLE_043_1 values('CDEFGHI','cdefghi',333,3.333,'2000-03-03 03:03:03');
insert into UNION_TABLE_043_1 values('DEFGHIJ','defghij',444,4.444,'2000-04-04 04:04:04');
insert into UNION_TABLE_043_1 values('EFGHIJK','efghijk',555,5.555,'2000-05-05 05:05:05');


create table UNION_TABLE_043_2(C_CHAR CHAR(103500),  C_VARCHAR VARCHAR(1035),  C_INT INTEGER not null,  C_DP double precision, C_TS_WITHOUT TIMESTAMP WITHOUT TIME ZONE)
partition by range (C_TS_WITHOUT)
( 
     partition UNION_TABLE_043_2_1  values less than ('2010-05-01'),
     partition UNION_TABLE_043_2_2  values less than ('2020-10-01')
);
insert into UNION_TABLE_043_2 values('ABCDEFG','abcdefg',111,1.111,'2000-01-01 01:01:01');
insert into UNION_TABLE_043_2 values('BCDEFGH','bcdefgh',222,2.222,'2010-02-02 02:02:02');
insert into UNION_TABLE_043_2 values('CDEFGHI','cdefghi',333,3.333,'2000-03-03 03:03:03');
insert into UNION_TABLE_043_2 values('DEFGHIJ','defghij',444,4.444,'2010-04-04 04:04:04');
insert into UNION_TABLE_043_2 values('EFGHIJK','efghijk',555,5.555,'2020-05-05 05:05:05');

select C_INT,C_DP,C_TS_WITHOUT from UNION_TABLE_043_1 union select C_INT,C_DP,C_TS_WITHOUT from UNION_TABLE_043_2 order by 1,2,3;

select C_INT,C_DP,C_TS_WITHOUT from UNION_TABLE_043_1 partition (UNION_TABLE_043_1_1) union select C_INT,C_DP,C_TS_WITHOUT from UNION_TABLE_043_2 partition (UNION_TABLE_043_2_1) order by 1,2,3;

drop table UNION_TABLE_043_1;
drop table UNION_TABLE_043_2;

drop schema FVT_COMPRESS cascade;

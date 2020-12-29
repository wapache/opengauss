\! gs_ktool -d all
\! gs_ktool -g
\! gs_ktool -g
\! gs_ktool -g

DROP CLIENT MASTER KEY IF EXISTS ImgCMK1 CASCADE;
DROP CLIENT MASTER KEY IF EXISTS ImgCMK CASCADE;

CREATE CLIENT MASTER KEY ImgCMK1 WITH ( KEY_STORE = gs_ktool , KEY_PATH = "gs_ktool/1" , ALGORITHM = AES_256_CBC);
CREATE CLIENT MASTER KEY ImgCMK WITH ( KEY_STORE = gs_ktool , KEY_PATH = "gs_ktool/2" , ALGORITHM = AES_256_CBC);

CREATE COLUMN ENCRYPTION KEY ImgCEK1 WITH VALUES (CLIENT_MASTER_KEY = ImgCMK1, ALGORITHM  = AEAD_AES_256_CBC_HMAC_SHA256);
CREATE COLUMN ENCRYPTION KEY ImgCEK WITH VALUES (CLIENT_MASTER_KEY = ImgCMK, ALGORITHM  = AEAD_AES_256_CBC_HMAC_SHA256);

CREATE TABLE creditcard_info (id_number    int, name         text encrypted with (column_encryption_key = ImgCEK, encryption_type = DETERMINISTIC),
credit_card  varchar(19) encrypted with (column_encryption_key = ImgCEK1, encryption_type = DETERMINISTIC));

INSERT INTO creditcard_info VALUES (1,'joe','6217986500001288393');
INSERT INTO creditcard_info VALUES (2, 'joy','6219985678349800033');
INSERT INTO creditcard_info VALUES (3, 'xiaoli', '6211877800001008888');
INSERT INTO creditcard_info VALUES (4, 'Nina', '6189486985800056893');
INSERT INTO creditcard_info VALUES (5, 'fanny', '7689458639568569354');
INSERT INTO creditcard_info VALUES (6, 'cora', '7584572945579384675');
INSERT INTO creditcard_info VALUES (7, 'nancy', '7497593456879650677');
CREATE TABLE creditcard_info1 (id_number    int, name         text encrypted with (column_encryption_key = ImgCEK, encryption_type = DETERMINISTIC),
credit_card  varchar(19) encrypted with (column_encryption_key = ImgCEK1, encryption_type = DETERMINISTIC));

INSERT INTO creditcard_info1 VALUES (1,'joe','6217986500001288393');
INSERT INTO creditcard_info1 VALUES (2, 'joy','6219985678349800033');
INSERT INTO creditcard_info1 VALUES (3, 'xiaoli', '6211877800001008888');
INSERT INTO creditcard_info1 VALUES (4, 'Nina', '6189486985800056893');
INSERT INTO creditcard_info1 VALUES (5, 'fanny', '7689458639568569354');
--支持
select * from creditcard_info1 where name = (select name from creditcard_info order by id_number limit 1);

CREATE TABLE creditcard_info2 (id_number    int, name1  text encrypted with (column_encryption_key = ImgCEK1, encryption_type = DETERMINISTIC),
name2  text encrypted with (column_encryption_key = ImgCEK1, encryption_type = DETERMINISTIC),
credit_card  varchar(19) encrypted with (column_encryption_key = ImgCEK1, encryption_type = DETERMINISTIC));

INSERT INTO creditcard_info2 VALUES (1,'joe','joe','6217986500001288393');
INSERT INTO creditcard_info2 VALUES (2, 'joy','joy','6219985678349800033');
INSERT INTO creditcard_info2 VALUES (3, 'xiaoli','xiaoli', '6211877800001008888');

CREATE TABLE creditcard_info3 (id_number    int, name1  text encrypted with (column_encryption_key = ImgCEK1, encryption_type = DETERMINISTIC),
name2  text encrypted with (column_encryption_key = ImgCEK, encryption_type = DETERMINISTIC),
credit_card  int encrypted with (column_encryption_key = ImgCEK1, encryption_type = DETERMINISTIC));
INSERT INTO creditcard_info3 VALUES (1,'joe','joe',62176500);
INSERT INTO creditcard_info3 VALUES (2, 'joy','joy',62199856);
INSERT INTO creditcard_info3 VALUES (3, 'xiaoli','xiaoli', 621187780);
explain INSERT INTO creditcard_info3 VALUES (3, 'xiaoli','xiaoli', 621187780);

--支持
select * from creditcard_info2 where name1 = (select name1 from creditcard_info3 order by id_number limit 1);
select * from (select * from creditcard_info3) where credit_card = 62176500;
select name2 from (select * from creditcard_info3) group by name1 ,name2 having name1 = 'joe';
select * from (select * from creditcard_info3 where credit_card = 62176500);
select * from (select * from creditcard_info3) as a , (select * from creditcard_info2) as b where a.credit_card = 62176500 and a.name1='joe' and b.name1='joe';
explain select * from (select * from creditcard_info3) as a , (select * from creditcard_info2) as b where a.credit_card = 62176500 and a.name1='joe' and b.name1='joe';

select credit_card, name1 
from 
(select name1,credit_card from creditcard_info3) as a , 
(select name2 from creditcard_info2) as b 
where name1='joe' and name2='joe'
group by credit_card, name1
having credit_card = 62176500;

-- select * from creditcard_info3 where exists (select * from creditcard_info2 where name1 = 'joe' and creditcard_info2.name1 = creditcard_info3.name1);
select * from creditcard_info3 where name1 in (select name1 from creditcard_info3 where name2 = 'joe');
select * from  creditcard_info3 as a , creditcard_info2 as b where a.credit_card = 62176500 and a.name1='joe' and b.name1='joe';
select * from (select name1,credit_card from creditcard_info3) as a , (select name2 from creditcard_info2) as b where a.credit_card = 62176500 and name1='joe' and name2='joe';

select name1 from  creditcard_info3  where credit_card = 62176500 INTERSECT select name1 from  creditcard_info2 ;
select id_number, name1 from  creditcard_info2  EXCEPT select id_number,name1 from  creditcard_info3 where credit_card = 62176500 order by id_number;

select * from (select name1 from  creditcard_info3  where credit_card = 62176500 INTERSECT select name1 from  creditcard_info2) where name1 ='joe';
select id_number, name1 from  creditcard_info3  INTERSECT select id_number,name2 from  creditcard_info2 order by id_number;
select id_number, name1 from  creditcard_info3   where credit_card = 62176500 UNION select id_number,name1 from  creditcard_info2 order by id_number;
select id_number, name1 from  creditcard_info3  UNION select id_number,name2 from  creditcard_info2 order by id_number;
select id_number, name2 from  creditcard_info3  INTERSECT select id_number,name2 from  creditcard_info2 order by id_number;
select id_number, name2 from  creditcard_info3  UNION select id_number,name2 from  creditcard_info2 order by id_number;

--不支持--输出为空
select * from creditcard_info2 where name1 = (select name from creditcard_info order by id_number limit 1);
select * from (select * from creditcard_info3) as a , (select * from creditcard_info2) as b where a.credit_card = 62176500 and a.name2='joe' and b.name2='joe';
select * from (select * from creditcard_info3) as a , (select * from creditcard_info2) as b where a.credit_card = 62176500 and a.name1='joe' and b.credit_card ='6217986500001288393';

DROP TABLE creditcard_info2;
DROP TABLE creditcard_info3;

CREATE TABLE creditcard_info2 (id_number    int, name1  text encrypted with (column_encryption_key = ImgCEK1, encryption_type = DETERMINISTIC),
name2  text encrypted with (column_encryption_key = ImgCEK1, encryption_type = RANDOMIZED),
credit_card  varchar(19) encrypted with (column_encryption_key = ImgCEK1, encryption_type = DETERMINISTIC));

INSERT INTO creditcard_info2 VALUES (1,'joe','joe','6217986500001288393');
INSERT INTO creditcard_info2 VALUES (2, 'joy','joy','6219985678349800033');
INSERT INTO creditcard_info2 VALUES (3, 'xiaoli','xiaoli', '6211877800001008888');

CREATE TABLE creditcard_info3 (id_number    int, name1  text encrypted with (column_encryption_key = ImgCEK1, encryption_type = DETERMINISTIC),
name2  text encrypted with (column_encryption_key = ImgCEK, encryption_type = DETERMINISTIC),
credit_card  int encrypted with (column_encryption_key = ImgCEK1, encryption_type = DETERMINISTIC));
INSERT INTO creditcard_info3 VALUES (1,'joe','joe',62176500);
INSERT INTO creditcard_info3 VALUES (2, 'joy','joy',62199856);
INSERT INTO creditcard_info3 VALUES (3, 'xiaoli','xiaoli', 621187780);

select name1 from  creditcard_info2   where name2 = 'joe';
select name1 from  creditcard_info2  INTERSECT select name2 from  creditcard_info2;
select name1 from  creditcard_info3  UNION select name2 from  creditcard_info2;
select name2 from  creditcard_info3  INTERSECT select name2 from  creditcard_info2;

CREATE TEMP TABLE creditcard_info4 (id_number    int, name1  text encrypted with (column_encryption_key = ImgCEK1, encryption_type = DETERMINISTIC),
name2  text encrypted with (column_encryption_key = ImgCEK1, encryption_type = RANDOMIZED),
credit_card  varchar(19) encrypted with (column_encryption_key = ImgCEK1, encryption_type = DETERMINISTIC));

INSERT INTO creditcard_info4 VALUES (1,'joe','joe','6217986500001288393');
INSERT INTO creditcard_info4 VALUES (2, 'joy','joy','6219985678349800033');
INSERT INTO creditcard_info4 VALUES (3, 'xiaoli','xiaoli', '6211877800001008888');


CREATE TEMP TABLE creditcard_info5 (id_number    int, name1  text encrypted with (column_encryption_key = ImgCEK1, encryption_type = DETERMINISTIC),
name2  text encrypted with (column_encryption_key = ImgCEK, encryption_type = DETERMINISTIC),
credit_card  int encrypted with (column_encryption_key = ImgCEK1, encryption_type = DETERMINISTIC));

INSERT INTO creditcard_info5 VALUES (1,'joe','joe',62176500);
INSERT INTO creditcard_info5 VALUES (2, 'joy','joy',62199856);
INSERT INTO creditcard_info5 VALUES (3, 'xiaoli','xiaoli', 621187780);

select * from creditcard_info4 where name1 = (select name1 from creditcard_info5 order by id_number limit 1);
select * from (select * from creditcard_info5) where credit_card = 62176500;
select name2 from (select * from creditcard_info5) group by name1 ,name2 having name1 = 'joe';
select * from (select * from creditcard_info5 where credit_card = 62176500);

begin;

CREATE TEMP TABLE creditcard_info7 (id_number    int, name1  text encrypted with (column_encryption_key = ImgCEK1, encryption_type = DETERMINISTIC),
name2  text encrypted with (column_encryption_key = ImgCEK, encryption_type = DETERMINISTIC),
credit_card  int encrypted with (column_encryption_key = ImgCEK1, encryption_type = DETERMINISTIC)) on commit preserve rows;

CREATE TEMP TABLE creditcard_info8 (id_number    int, name1  text encrypted with (column_encryption_key = ImgCEK1, encryption_type = DETERMINISTIC),
name2  text encrypted with (column_encryption_key = ImgCEK, encryption_type = DETERMINISTIC),
credit_card  int encrypted with (column_encryption_key = ImgCEK1, encryption_type = DETERMINISTIC)) on commit delete rows;

INSERT INTO creditcard_info7 VALUES (1,'joe','joe',62176500);
INSERT INTO creditcard_info8 VALUES (2, 'joy','joy',62199856);

commit;

select * from creditcard_info7;
select * from creditcard_info8;


DROP TABLE creditcard_info;
DROP TABLE creditcard_info1;
DROP TABLE creditcard_info2;
DROP TABLE creditcard_info3;


CREATE CLIENT MASTER KEY lidj_cmk WITH ( KEY_STORE = gs_ktool , KEY_PATH = "gs_ktool/3" , ALGORITHM = AES_256_CBC);
CREATE COLUMN ENCRYPTION KEY lidj_cek WITH VALUES (CLIENT_MASTER_KEY = lidj_cmk, ALGORITHM  = AEAD_AES_256_CBC_HMAC_SHA256);
BEGIN;
DROP CLIENT MASTER KEY IF EXISTS lidj_cmk CASCADE;
ROLLBACK;

CREATE TABLE creditcard_info (id_number  int, name  text encrypted with (column_encryption_key = lidj_cek, encryption_type = DETERMINISTIC));
INSERT INTO creditcard_info VALUES (1,'joe');
INSERT INTO creditcard_info VALUES (2, 'joy');
INSERT INTO creditcard_info VALUES (3, 'xiaoli');
INSERT INTO creditcard_info VALUES (4, 'Nina');
INSERT INTO creditcard_info VALUES (5, 'fanny');
INSERT INTO creditcard_info VALUES (6, 'cora');
INSERT INTO creditcard_info VALUES (7, 'nancy');
select * from creditcard_info order by id_number;
BEGIN;
DROP TABLE creditcard_info;
DROP CLIENT MASTER KEY IF EXISTS lidj_cmk CASCADE;
ROLLBACK;
select * from creditcard_info order by id_number;

DROP TABLE creditcard_info;
DROP TABLE creditcard_info4;
DROP TABLE creditcard_info5;
DROP TABLE IF EXISTS creditcard_info7;
DROP TABLE IF EXISTS creditcard_info8;

DROP COLUMN ENCRYPTION KEY ImgCEK1;
DROP COLUMN ENCRYPTION KEY ImgCEK;
DROP COLUMN ENCRYPTION KEY lidj_cek;
DROP CLIENT MASTER KEY ImgCMK1;
DROP CLIENT MASTER KEY ImgCMK;
DROP CLIENT MASTER KEY lidj_cmk;
SELECT * FROM gs_client_global_keys;
SELECT * FROM gs_client_global_keys_args;
SELECT * FROM gs_column_keys;
SELECT * FROM gs_column_keys_args;

select  count(*), 'count' from gs_client_global_keys;
select  count(*), 'count' from gs_column_keys;

\! gs_ktool -d all

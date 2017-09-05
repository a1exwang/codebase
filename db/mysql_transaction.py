import pymysql


# MySQL transaction ACID test
# Atomic, Consistent, Isolated, Durable

def connect():
    db = pymysql.connect(host="localhost", user="root", passwd="123", db="test_xa", autocommit=True)
    return db


def ensure_schema(conn):
    with conn.cursor() as c:
        c.execute("""
            DROP TABLE IF EXISTS t1;
            DROP TABLE IF EXISTS t2;
            CREATE TABLE IF NOT EXISTS t1 (id int PRIMARY KEY, code VARCHAR(32), name VARCHAR(100));
            CREATE UNIQUE INDEX idx_t1_code ON t1 (code);
            CREATE TABLE IF NOT EXISTS t2 (id int PRIMARY KEY, code VARCHAR(32), name VARCHAR(100));
            INSERT INTO t1(id, code, name) VALUES
                (1, '1', 'one'),
                (2, '2', 'two'),
                (3, '3', 'three'),
                (4, '4', 'four'),
                (6, '6', 'six'),
                (7, '7', 'seven'),
                (8, '8', 'eight');
        """)


def test_isolation_level_repeatable_read_for_indexed_col(db1, db2):
    c1 = db1.cursor()
    c2 = db2.cursor()

    c2.execute("START TRANSACTION;")
    n1 = c2.execute("SELECT * FROM t1 WHERE id >= 4;")
    c1.execute("INSERT INTO t1 VALUES (5, '5', 'five');")
    # In repeatable read isolation level, this should return 4
    n2 = c2.execute("SELECT * FROM t1 WHERE id >= 4;")
    c2.execute("COMMIT;")
    n3 = c2.execute("SELECT * FROM t1 WHERE id >= 4;")
    assert n1 == n2
    assert n1 + 1 == n3

    c1.close()
    c2.close()


def test_isolation_level_repeatable_read_for_unindexed_col(db1, db2):
    c1 = db1.cursor()
    c2 = db2.cursor()

    c2.execute("START TRANSACTION;")
    n1 = c2.execute("SELECT * FROM t1 WHERE name < 'one';")
    c1.execute("INSERT INTO t1 VALUES (5, '5', 'five');")
    # In repeatable read isolation level, this should return 4
    n2 = c2.execute("SELECT * FROM t1 WHERE name < 'one';")
    c2.execute("COMMIT;")
    n3 = c2.execute("SELECT * FROM t1 WHERE name < 'one';")
    assert n1 == n2
    assert n1 + 1 == n3


def test_lock(db1, db2):
    c1 = db1.cursor()
    c2 = db2.cursor()

    c2.execute("START TRANSACTION;")
    c2.execute("SELECT name FROM t1 WHERE id = 4 FOR UPDATE;")
    name = c2.fetchone()[0];

    # Should deadlock
    #  c1.execute("UPDATE t1 SET name = '' WHERE id = 4;")

    c2.execute("UPDATE t1 SET name = %s WHERE id = 4;", name + ' a')
    c2.execute("COMMIT;")

    c1.close()
    c2.close()


def test_phantom_read(db1, db2):
    c1 = db1.cursor()
    c2 = db2.cursor()

    c2.execute("START TRANSACTION;")
    c2.execute("SELECT id FROM t1 WHERE id >= 4;")
    ids = [x[0] for x in c2.fetchall()]
    c1.execute("INSERT INTO t1 VALUES (5, '5', 'five');")
    # In repeatable read isolation level, this should return 4
    ids_str = ','.join([str(x) for x in ids])
    c2.execute("UPDATE t1 SET name = 'wtf' WHERE id IN (%s);" % ids_str)
    c2.execute("COMMIT;")
    r = c2.execute("SELECT COUNT(*) FROM t1 WHERE id >= 4;")
    print("Should be 1 if phantom read occurs, value = %s" % r)

    c1.close()
    c2.close()


db1 = connect()
db2 = connect()
ensure_schema(db1)
test_isolation_level_repeatable_read_for_indexed_col(db1, db2)

ensure_schema(db1)
test_isolation_level_repeatable_read_for_unindexed_col(db1, db2)

ensure_schema(db1)
test_lock(db1, db2)

ensure_schema(db1)
test_phantom_read(db1, db2)

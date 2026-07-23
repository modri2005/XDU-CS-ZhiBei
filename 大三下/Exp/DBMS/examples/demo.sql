DROP TABLE IF EXISTS Student;
CREATE TABLE Student (
    Sno CHAR(9),
    Sname CHAR(20),
    Ssex CHAR(2),
    Sage INT
);

INSERT INTO Student VALUES ('202600001', 'Alice', 'F', 20);
INSERT INTO Student VALUES ('202600002', 'Bob', 'M', 21);
INSERT INTO Student VALUES ('202600003', 'Carol', 'F', 19);
INSERT INTO Student VALUES ('202600004', 'David', 'M', 22);
INSERT INTO Student VALUES ('202600005', 'Eva', 'F', 21);
INSERT INTO Student VALUES ('202600006', 'Frank', 'M', 18);

SHOW TABLES;
DESC Student;
SELECT * FROM Student;
SELECT Sno, Sname, Sage FROM Student WHERE Sage >= 20 AND Ssex = 'F';
SELECT Sno, Sname, Ssex FROM Student WHERE Sage < 21;
SELECT Sno, Sname, Sage FROM Student WHERE Sage <> 19;
SELECT * FROM Student WHERE Sname = 'David';
SELECT Sno, Sname, Sage FROM Student WHERE Sage >= 21 AND Ssex = 'M';
UPDATE Student SET Sage = 22 WHERE Sname = 'Bob';
UPDATE Student SET Ssex = 'F' WHERE Sname = 'Eva';
SELECT * FROM Student WHERE Sage >= 21;
SELECT * FROM Student WHERE Ssex = 'F' AND Sage >= 20;
DELETE FROM Student WHERE Sno = '202600003';
DELETE FROM Student WHERE Sage < 20;
SELECT * FROM Student;
DROP TABLE Student;

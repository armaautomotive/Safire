
CREATE DATABASE safire_nodes;

CREATE TABLE node_list (
     id MEDIUMINT NOT NULL AUTO_INCREMENT,
     name VARCHAR(512),
     public_address VARCHAR(512),
     connection_string VARCHAR(1024),
     time timestamp  DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
     PRIMARY KEY (id)
);


mysql> describe node_list;
+-------------------+---------------+------+-----+-------------------+-----------------------------+
| Field             | Type          | Null | Key | Default           | Extra                       |
+-------------------+---------------+------+-----+-------------------+-----------------------------+
| id                | mediumint(9)  | NO   | PRI | NULL              | auto_increment              |
| name              | varchar(512)  | YES  |     | NULL              |                             |
| public_address    | varchar(512)  | YES  |     | NULL              |                             |
| connection_string | text          | YES  |     | NULL              |                             |
| time              | timestamp     | NO   |     | CURRENT_TIMESTAMP | on update CURRENT_TIMESTAMP |
| public_key        | varchar(1024) | YES  |     | NULL              |                             |
+-------------------+---------------+------+-----+-------------------+-----------------------------+

mysql> describe messages;
+--------------+--------------+------+-----+-------------------+----------------+
| Field        | Type         | Null | Key | Default           | Extra          |
+--------------+--------------+------+-----+-------------------+----------------+
| id           | bigint(20)   | NO   | PRI | NULL              | auto_increment |
| sender_key   | varchar(255) | YES  |     | NULL              |                |
| receiver_key | varchar(255) | YES  |     | NULL              |                |
| request      | varchar(255) | YES  |     | NULL              |                |
| message      | text         | YES  |     | NULL              |                |
| time         | timestamp    | NO   |     | CURRENT_TIMESTAMP |                |
+--------------+--------------+------+-----+-------------------+----------------+


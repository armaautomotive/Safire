
CREATE DATABASE safire_nodes;

CREATE TABLE node_list (
     id MEDIUMINT NOT NULL AUTO_INCREMENT,
     name VARCHAR(512),
     public_address VARCHAR(512),
     connection_string VARCHAR(1024),
     time timestamp  DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
     PRIMARY KEY (id)
);




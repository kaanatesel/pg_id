CREATE TABLE accounts (
	id serial PRIMARY KEY,
    name varchar(50),
    mongo_id mgid
);

INSERT INTO accounts(id, name, mongo_id) values (4,'Linus Torvalds','507f1f77bcf86cd799439014');
INSERT INTO accounts(id, name, mongo_id) values (8,'Bjarne Stroustrup','507f1f77bcf86cd799439018');
INSERT INTO accounts(id, name, mongo_id) values (2,'Tim Berners-Lee','507f1f77bcf86cd799439012');
INSERT INTO accounts(id, name, mongo_id) values (7,'Aaron Swartz','507f1f77bcf86cd799439017');
INSERT INTO accounts(id, name, mongo_id) values (1,'Dennis MacAlistair Ritchie','507f1f77bcf86cd799439011');
INSERT INTO accounts(id, name, mongo_id) values (6,'Guido van Rossum','507f1f77bcf86cd799439016');
INSERT INTO accounts(id, name, mongo_id) values (3,'James Gosling','507f1f77bcf86cd799439013');
INSERT INTO accounts(id, name, mongo_id) values (5,'Richard Stallman','507f1f77bcf86cd799439015');

select * from accounts order by mongo_id;
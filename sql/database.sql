drop database if exists gomoku;
create database if not exists gomoku;
use gomoku;
create table if not exists user(
    id int primary key auto_increment,
    username varchar(32) unique key not null,
    password varchar(256) not null,
    score int,
    pk_cnt int,
    win_cnt int
);

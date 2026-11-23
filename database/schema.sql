CREATE DATABASE IF NOT EXISTS energytrade; USE energytrade;
CREATE TABLE users (user_id VARCHAR(30) PRIMARY KEY, role ENUM('buyer','seller') NOT NULL, created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP);
CREATE TABLE buy_orders (order_id INT PRIMARY KEY, user_id VARCHAR(30), bid_price DECIMAL(8,2) CHECK (bid_price>0), quantity INT CHECK (quantity>0), remaining INT, created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP);
CREATE TABLE sell_orders (order_id INT PRIMARY KEY, user_id VARCHAR(30), ask_price DECIMAL(8,2) CHECK (ask_price>0), quantity INT CHECK (quantity>0), remaining INT, created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP);
CREATE TABLE trades (trade_id INT PRIMARY KEY, buyer_id VARCHAR(30), seller_id VARCHAR(30), price DECIMAL(8,2), quantity INT, total_value DECIMAL(12,2), executed_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP);

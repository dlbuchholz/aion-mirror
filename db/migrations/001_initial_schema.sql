CREATE TABLE IF NOT EXISTS users (
    id INTEGER PRIMARY KEY,
    username TEXT NOT NULL UNIQUE,
    name TEXT NOT NULL UNIQUE
);

CREATE TABLE IF NOT EXISTS accounts (
    id INTEGER PRIMARY KEY,
    user_id INTEGER NOT NULL,
    name TEXT NOT NULL,
    type_id INTEGER NOT NULL,
    FOREIGN KEY (user_id) REFERENCES users (id) ON DELETE CASCADE,
	FOREIGN KEY (type_id) REFERENCES account_types (id) ON DELETE RESTRICT
);

CREATE TABLE account_types (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL UNIQUE
);

CREATE TABLE IF NOT EXISTS transactions (
    id INTEGER PRIMARY KEY,
    account_id INTEGER NOT NULL,
    payee TEXT,
    amount REAL NOT NULL,
    date TEXT NOT NULL,
    description TEXT,
    category_id INTEGER,
    type TEXT NOT NULL CHECK (type IN ('income', 'expense')),
    FOREIGN KEY (account_id) REFERENCES accounts (id) ON DELETE CASCADE,
	FOREIGN KEY (category) REFERENCES categories (name)
);

CREATE TABLE IF NOT EXISTS categories (
    id INTEGER PRIMARY KEY,
    name TEXT NOT NULL UNIQUE,
    type TEXT NOT NULL CHECK (type IN ('income', 'expense'))
);

CREATE TABLE IF NOT EXISTS budgets (
    id INTEGER PRIMARY KEY,
    user_id INTEGER NOT NULL,
    category TEXT NOT NULL,
    amount REAL NOT NULL,
    start_date TEXT NOT NULL,
    end_date TEXT NOT NULL,
    FOREIGN KEY (user_id) REFERENCES users (id) ON DELETE CASCADE,
    FOREIGN KEY (category) REFERENCES categories (name)
);

CREATE TABLE IF NOT EXISTS recurring_transactions (
    id INTEGER PRIMARY KEY,
    transaction_id INTEGER NOT NULL,
    frequency TEXT NOT NULL CHECK (frequency IN ('daily', 'weekly', 'monthly', 'yearly')),
    FOREIGN KEY (transaction_id) REFERENCES transactions (id) ON DELETE CASCADE
);

CREATE INDEX idx_transactions_account_id ON transactions(account_id);

CREATE VIEW account_balances AS
SELECT 
    accounts.id AS account_id,
    accounts.name AS account_name,
    account_types.name AS account_type,
    accounts.user_id AS user_id,
    COALESCE(SUM(transactions.amount), 0) AS balance
FROM accounts
LEFT JOIN account_types ON accounts.type_id = account_types.id
LEFT JOIN transactions ON accounts.id = transactions.account_id
GROUP BY accounts.id;

INSERT INTO account_types (name) VALUES
('Checking'),
('Savings'),
('Investment'),
('Retirement'),
('Virtual');

INSERT INTO categories (name, type) VALUES
('Initial balance', 'income'),
('Salary', 'income'),
('Bonus', 'income'),
('Groceries', 'expense'),
('Rent', 'expense'),
('Utilities', 'expense'),
('Entertainment', 'expense');
CREATE TABLE IF NOT EXISTS users (
    id INTEGER PRIMARY KEY,
    username TEXT NOT NULL UNIQUE,
    name TEXT NOT NULL UNIQUE
);

-- Settings that belong to the data rather than to the computer, like the currency and
-- number format. Keys as in QSettings ('currency/symbol', ...)
CREATE TABLE IF NOT EXISTS database_settings (
    key TEXT PRIMARY KEY,
    value TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS account_types (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
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

CREATE TABLE IF NOT EXISTS categories (
    id INTEGER PRIMARY KEY,
    name TEXT NOT NULL UNIQUE,
    type TEXT NOT NULL CHECK (type IN ('income', 'expense')),
    color TEXT                           -- Palette name ('teal', 'coral', ...), NULL = neutral
);

CREATE TABLE IF NOT EXISTS transactions (
    id INTEGER PRIMARY KEY,
    account_id INTEGER NOT NULL,
    payee TEXT,
    amount INTEGER NOT NULL,             -- In cents
    date TEXT NOT NULL,
    description TEXT,
    category_id INTEGER,
    type TEXT NOT NULL CHECK (type IN ('income', 'expense')),
    hash TEXT,
    FOREIGN KEY (account_id) REFERENCES accounts (id) ON DELETE CASCADE,
	FOREIGN KEY (category_id) REFERENCES categories (id)
);

-- Monthly spending limits per category. A repeating row applies to every month from
-- start_month until end_month (NULL = open-ended). A non-repeating row applies to
-- start_month only and takes precedence over a repeating one in that month.
CREATE TABLE IF NOT EXISTS budgets (
    id INTEGER PRIMARY KEY,
    user_id INTEGER NOT NULL,
    category_id INTEGER NOT NULL,
    amount INTEGER NOT NULL,             -- Limit in cents
    start_month TEXT NOT NULL,           -- 'YYYY-MM'
    end_month TEXT,                      -- 'YYYY-MM', inclusive, NULL = no end
    repeats INTEGER NOT NULL DEFAULT 1 CHECK (repeats IN (0, 1)),
    FOREIGN KEY (user_id) REFERENCES users (id) ON DELETE CASCADE,
    FOREIGN KEY (category_id) REFERENCES categories (id)
);
CREATE UNIQUE INDEX IF NOT EXISTS idx_budgets_unique ON budgets(user_id, category_id, start_month, repeats);

CREATE TABLE IF NOT EXISTS recurring_transactions (
    id INTEGER PRIMARY KEY,
    transaction_id INTEGER NOT NULL,
    frequency TEXT NOT NULL CHECK (frequency IN ('daily', 'weekly', 'monthly', 'yearly')),
    FOREIGN KEY (transaction_id) REFERENCES transactions (id) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_transactions_account_id ON transactions(account_id);
--Create index for fast duplicate lookups, and unique constraint to prevent duplicates at DB level
--CREATE UNIQUE INDEX IF NOT EXISTS idx_transactions_unique_hash ON transactions(hash);
CREATE INDEX IF NOT EXISTS idx_transactions_hash ON transactions(hash);

CREATE VIEW IF NOT EXISTS account_balances AS
SELECT 
    accounts.id AS account_id,
    accounts.name AS account_name,
    account_types.name AS account_type,
    accounts.user_id AS user_id,
    COALESCE(
        SUM(
            CASE 
                WHEN transactions.type = 'income' THEN transactions.amount
                WHEN transactions.type = 'expense' THEN -transactions.amount
                ELSE 0
            END
        ), 0
    ) AS balance
FROM accounts
LEFT JOIN account_types ON accounts.type_id = account_types.id
LEFT JOIN transactions ON accounts.id = transactions.account_id
GROUP BY accounts.id;

INSERT OR IGNORE INTO account_types (name) VALUES
('Checking'),
('Savings'),
('Investment'),
('Retirement'),
('Virtual');

INSERT OR IGNORE INTO categories (name, type) VALUES
('Initial balance', 'income'),
('Salary', 'income'),
('Bonus', 'income'),
('Groceries', 'expense'),
('Rent', 'expense'),
('Utilities', 'expense'),
('Entertainment', 'expense'),
('Balance correction', 'income');

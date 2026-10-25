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

-- Account types and categories belong to a user. New users get the default ones
-- (AccountDAO::addDefaultAccountTypes, CategoryDAO::addDefaultCategories).
CREATE TABLE IF NOT EXISTS account_types (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id INTEGER NOT NULL,
    name TEXT NOT NULL,
    UNIQUE (user_id, name),
    FOREIGN KEY (user_id) REFERENCES users (id) ON DELETE CASCADE
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
    user_id INTEGER NOT NULL,
    name TEXT NOT NULL,
    type TEXT NOT NULL CHECK (type IN ('income', 'expense')),
    color TEXT,                          -- Palette name ('teal', 'coral', ...), NULL = neutral
    UNIQUE (user_id, name),
    FOREIGN KEY (user_id) REFERENCES users (id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS transactions (
    id INTEGER PRIMARY KEY,
    account_id INTEGER NOT NULL,
    payee TEXT,
    original_payee TEXT,                 -- The payee before a rule renamed it, NULL = never renamed
    renamed_by_rule INTEGER,             -- The rule that gave the payee its name, NULL = none (or changed by hand since)
    amount INTEGER NOT NULL,             -- In cents
    date TEXT NOT NULL,
    description TEXT,
    category_id INTEGER,
    type TEXT NOT NULL CHECK (type IN ('income', 'expense')),
    hash TEXT,
    FOREIGN KEY (account_id) REFERENCES accounts (id) ON DELETE CASCADE,
	FOREIGN KEY (category_id) REFERENCES categories (id),
    FOREIGN KEY (renamed_by_rule) REFERENCES rules (id) ON DELETE SET NULL
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

-- Rules give transactions a category and/or rename their payee: when all (or any) of a rule's
-- conditions match. A user's rules are tried in position order; for each action the first
-- matching rule that has it wins. Deleting a category takes it out of its rules (the app deletes
-- rules that then do nothing).
CREATE TABLE IF NOT EXISTS rules (
    id INTEGER PRIMARY KEY,
    user_id INTEGER NOT NULL,
    position INTEGER NOT NULL,
    match_all INTEGER NOT NULL DEFAULT 1 CHECK (match_all IN (0, 1)),
    category_id INTEGER,                 -- NULL = the rule doesn't set a category
    new_payee TEXT,                      -- NULL = the rule doesn't rename the payee
    FOREIGN KEY (user_id) REFERENCES users (id) ON DELETE CASCADE,
    FOREIGN KEY (category_id) REFERENCES categories (id) ON DELETE SET NULL
);

-- One condition of a rule. Text is compared without case; amounts are positive cents. "between"
-- uses value and value2 (both included). Type is 'income' or 'expense', account an account id.
CREATE TABLE IF NOT EXISTS rule_conditions (
    id INTEGER PRIMARY KEY,
    rule_id INTEGER NOT NULL,
    field TEXT NOT NULL CHECK (field IN ('payee', 'description', 'payee_or_description', 'amount', 'type', 'account')),
    operator TEXT NOT NULL CHECK (operator IN ('contains', 'is', 'starts_with', 'between')),
    value TEXT NOT NULL,
    value2 TEXT,
    FOREIGN KEY (rule_id) REFERENCES rules (id) ON DELETE CASCADE
);

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

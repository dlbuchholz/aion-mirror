-- The account types and categories a user starts with follow the language of the texts until
-- the user renames them. "starter" holds the English name, which is translated on every start
-- (StarterNames::followLanguage); NULL for the user's own and for renamed ones.
ALTER TABLE account_types ADD COLUMN starter TEXT;
ALTER TABLE categories ADD COLUMN starter TEXT;

-- Databases from before translations have the starters in English, as they were created
UPDATE account_types SET starter = name
WHERE name IN ('Checking', 'Savings', 'Investment', 'Retirement', 'Virtual');
UPDATE categories SET starter = name
WHERE name IN ('Salary', 'Bonus', 'Groceries', 'Rent', 'Utilities', 'Entertainment');

# sqlite3-arabic-tokenizer

This is a custom [FTS5](https://www.sqlite.org/fts5.html) tokenizer for sqlite.If you don't know what is a custom
tokenizer, you can check [this](https://www.sqlite.org/fts5.html#custom_tokenizers).

This tokenizer will remove diacritics from arabic text. Suppose, you have a quran app where you want to search without
arabic diacritics. Like you want to match 'الحمد' with 'ٱلْحَمْدُ'. You can use this tokenizer to create a virtual fts5
table that will do this.

## Usage

It can be used anywhere where sqlite with fts5 is supported.

### From terminal

```
sqlite> .open /Users/gtaf/quran.db
sqlite> select load_extension('/Users/gtaf/sqlite3-arabic-tokenizer/dist/x86/sqlite3-arabic-tokenizer');
sqlite> CREATE VIRTUAL TABLE IF NOT EXISTS ar USING fts5 (sura, ayah, text, primary, tokenize='arabic_tokenizer');
sqlite> INSERT INTO ar VALUES (1, 2, 'ٱلْحَمْدُ لِلَّهِ رَبِّ ٱلْعَٰلَمِينَ', NULL);
sqlite> SELECT * from ar WHERE text MATCH 'الحمد';
1|2|ٱلْحَمْدُ لِلَّهِ رَبِّ ٱلْعَٰلَمِينَ |
sqlite>
```
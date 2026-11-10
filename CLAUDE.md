# Instructions for Claude

## Simplified Technical English

Write all text in ASD-STE100 Simplified Technical English (STE), issue 9.
This instruction is for these texts:

- The replies in the chat
- The documents, for example the README files
- The comments in the code
- The commit messages

For the rules, refer to `Docs/ASD-STE100_ISSUE9.pdf`. Part 1 ("Writing
rules") gives the rules. Part 2 is the dictionary. To find a word quickly,
use the text copy `Docs/ASD-STE100_ISSUE9.txt`.

### Words

- Use only approved words, technical nouns, and technical verbs.
- Use an approved word only as the part of speech that the dictionary
  gives, and only with its approved meaning. For example, "test" is an
  approved noun, but it is not an approved verb. Thus, write "do a test".
- Use only the verb forms that the dictionary gives.
- `Docs/ste-glossary.md` gives the technical nouns and the technical verbs
  of this project. Use one term for one item.
- Before you use a new technical noun or technical verb, add it to the
  glossary.
- Write words as in American English, for example "color" and not
  "colour".

### Verbs

- Use only these verb forms:
  - The infinitive
  - The imperative
  - The simple present tense
  - The simple past tense
  - The simple future tense
  - The past participle as an adjective
- Use the active voice. In a description, use the passive voice only if
  the person or the item that does the work is unknown.
- Use the "-ing" verb form only in a technical noun.
- Do not use phrasal verbs.

### Sentences and paragraphs

- In a procedure, write a maximum of 20 words in each sentence.
- In a description, write a maximum of 25 words in each sentence.
- Write only one instruction in each sentence.
- Write each instruction in the imperative.
- If the reader must know about a condition first, write the condition
  first. Then write a comma and the instruction.
- Do not remove words to make a sentence shorter.
- Do not use contractions.
- Use an article (the, a, an) or a demonstrative adjective (this, these)
  before a noun when applicable.
- Use a vertical list when a text has many items or steps.
- Make sure that each paragraph has only one topic.
- Write a maximum of 6 sentences in each paragraph.
- Do not use semicolons.

### Code

- Use STE in the comments, not in the code. Do not change identifiers,
  macros, register names, or string literals because of STE.
- When you change a comment, keep all the information in it. If a
  sentence is too long, write two sentences or a list.

### Examine your text

The checker shows possible problems in a file:

```
python Docs/ste_check.py FILE...
```

The checker cannot find the part of speech or the meaning of a word. Thus,
examine each result with the dictionary. For the dictionary data about one
word, use `python Docs/ste_check.py --lookup WORD`.

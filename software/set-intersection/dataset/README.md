# Set Intersection Dataset

Download the latest Wikipedia dump:

    wget https://dumps.wikimedia.org/enwiki/latest/enwiki-latest-pages-articles.xml.bz2

Extract the corpus with [make_wiki_corpus.py](https://gist.github.com/mmmayo13/0f2ad1b0f7f83810514687c4ef61032e#file-make_wiki_corpus-py):

    python3 make_wiki_corpus.py enwiki-latest-pages-articles.xml.bz2 corpus-en.txt

Generate the top English word list without stopwords:

    wget https://gist.github.com/deekayen/4148741/raw/98d35708fa344717d8eee15d11987de6c8e26d7d/1-1000.txt
    cat 1-1000.txt | python clean_wordlist.py | tail -n 200 > words.txt

Create the index:

    make
    ./make_wiki_index

This generates `word_to_docids.h` which should be copied into the parent
directory before compiling `../lnic-euclidean-dist.cc`.

## References

1. [Building a Wikipedia Text Corpus for Natural Language Processing](https://www.kdnuggets.com/2017/11/building-wikipedia-text-corpus-nlp.html)
2. [make_wiki_corpus.py](https://gist.github.com/mmmayo13/0f2ad1b0f7f83810514687c4ef61032e#file-make_wiki_corpus-py)
3. [Error training on wikipedia dump](https://groups.google.com/g/gensim/c/7k0_ICYuYqg)

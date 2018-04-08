# Dictionary

A dictionary component with anagram support.

## Design Notes

The anagram requirement drove all of the design decisions from the get go.  The
dictionary has two top-level AVL tables: one for bitmaps and one for string
lengths.

When a string is added to the dictionary via [AddWord](https://github.com/tpn/dictionary/blob/v0.7/Dictionary/AddWord.c#L538)
(which wraps the internal [AddWordEntry](https://github.com/tpn/dictionary/blob/v0.7/Dictionary/AddWord.c#L20) routine),
a "word initializer" routine is called ([InitializeWord](https://github.com/tpn/dictionary/blob/v0.7/Dictionary/Word.c#L20)),
which [loops through each byte of the incoming array](https://github.com/tpn/dictionary/blob/v0.7/Dictionary/Word.c#L170)
and does two things: 1) sets a bit in a bitmap corresponding to the byte's
value (e.g. the character 'f' would set bit 102), and 2) increments a counter
in a 256 element array of ULONGs, where each element also corresponds to a byte
value.  (So, continuing the example, if 'f' were the first character, the ULONG
residing at position 102 in the array would be incremented to 1.)

The bitmap is a 256-bit, 32-byte structure named [CHARACTER_BITMAP](https://github.com/tpn/dictionary/blob/v0.7/Dictionary/DictionaryPrivate.h#L63), which is defined as follows:

```c
    typedef union DECLSPEC_ALIGN(32) _CHARACTER_BITMAP {
         YMMWORD Ymm;
         XMMWORD Xmm[2];
         LONG Bits[8];
    } CHARACTER_BITMAP;
    C_ASSERT(sizeof(CHARACTER_BITMAP) == 32);
    typedef CHARACTER_BITMAP *PCHARACTER_BITMAP;
    typedef const CHARACTER_BITMAP *PCCHARACTER_BITMAP;
```

The histogram is a 1024 byte structure named [CHARACTER_HISTOGRAM](https://github.com/tpn/dictionary/blob/v0.7/Dictionary/DictionaryPrivate.h#L72), which is defined as follows:

```c
    typedef union DECLSPEC_ALIGN(32) _CHARACTER_HISTOGRAM {
        YMMWORD Ymm[32];
        XMMWORD Xmm[64];
        ULONG Counts[NUMBER_OF_CHARACTER_BITS];
    } CHARACTER_HISTOGRAM;
    C_ASSERT(sizeof(CHARACTER_HISTOGRAM) == 1024);
    typedef CHARACTER_HISTOGRAM *PCHARACTER_HISTOGRAM;
    typedef const CHARACTER_HISTOGRAM *PCCHARACTER_HISTOGRAM;
```

DECLSPEC_ALIGN(32) is used to inform the compiler that the structures should be
aligned on 32-byte boundaries.  This is done to permit the use of optimal AVX2
instructions (e.g. for doing aligned loads into YMM registers).

The union approach (where different representations of the same underlying data
are bundled together and can be accessed at the same memory offset) is used here
(and frequently throughout) to make working with the structures more convenient.

The histogram is used because it allows us to efficiently determine if a word is
an anagram of another word.  If the histograms are equivalent, the two words are
anagrams.

Thus, if we want an efficient way to obtain all of the anagrams for a given
word, we can calculate the histogram for that word, look the histogram up in a
table data structure, and then simply enumerate all of the words that are
associated with that histogram.

The bitmap is essentially a more condensed version of the information presented
in the histogram; in only 32 bytes, we can capture a lot of the identifying
information about an incoming word in lieu of the 1024 bytes required for the
histogram.

Another nice property of the bitmap is that it can effectively be used as a
bloom filter when determining whether or not a word is present in a dictionary.
If a bitmap is calculated for the incoming word, we can compare this bitmap to
a table of known bitmaps.  If no matching bitmap is found, we can be assured
no such word exists in the dictionary, without having to search any histograms.

So, I decided to make bitmaps the top-level AVL tree structure.  The private
[DICTIONARY](https://github.com/tpn/dictionary/blob/v0.7/Dictionary/DictionaryPrivate.h#L529)
structure embeds a [BITMAP_TABLE](https://github.com/tpn/dictionary/blob/v0.7/Dictionary/DictionaryPrivate.h#L244) structure, which is simply an [RTL_AVL_TABLE](https://github.com/tpn/dictionary/blob/v0.7/Dictionary/DictionaryPrivate.h#L131) structure.  A bitmap table entry is simply a wrapper around a [HISTOGRAM_TABLE](https://github.com/tpn/dictionary/blob/v0.7/Dictionary/DictionaryPrivate.h#L244)

```c
    typedef struct _BITMAP_TABLE {
        RTL_AVL_TABLE Avl;
    } BITMAP_TABLE;
    typedef BITMAP_TABLE *PBITMAP_TABLE;

    typedef struct _BITMAP_TABLE_ENTRY {
        HISTOGRAM_TABLE HistogramTable;
    } BITMAP_TABLE_ENTRY;
    typedef BITMAP_TABLE_ENTRY *PBITMAP_TABLE_ENTRY;
```

Likewise, the HISTOGRAM_TABLE is simply an RTL_AVL_TREE, with each entry
embedding a WORD_TABLE:

```c
    typedef struct _HISTOGRAM_TABLE {
        RTL_AVL_TABLE Avl;
    } HISTOGRAM_TABLE;
    typedef HISTOGRAM_TABLE *PHISTOGRAM_TABLE;

    typedef struct _HISTOGRAM_TABLE_ENTRY {
        WORD_TABLE WordTable;
    } HISTOGRAM_TABLE_ENTRY;
    typedef HISTOGRAM_TABLE_ENTRY *PHISTOGRAM_TABLE_ENTRY;
```

The WORD_TABLE is actually a hash table.  Kidding, it's also an AVL table:

```c
    typedef struct _WORD_TABLE {
        RTL_AVL_TABLE Avl;
    } WORD_TABLE;
    typedef WORD_TABLE *PWORD_TABLE;

    typedef struct _WORD_TABLE_ENTRY {
        WORD_ENTRY WordEntry;
        LIST_ENTRY LengthListEntry;
    } WORD_TABLE_ENTRY;
    typedef WORD_TABLE_ENTRY *PWORD_TABLE_ENTRY;
```

The WORD_TABLE captures WORD_TABLE_ENTRY structures, where we finally see the
actual underlying words that were added to the dictionary.

So, if we have an empty dictionary and we add the word "elbow" to it, we first
need to [create a bitmap table entry](https://github.com/tpn/dictionary/blob/v0.7/Dictionary/AddWord.c#L200)
for it and [initialize a new histogram table](https://github.com/tpn/dictionary/blob/v0.7/Dictionary/AddWord.c#L200)
at the resulting entry.

We then [add a histogram table entry](https://github.com/tpn/dictionary/blob/v0.7/Dictionary/AddWord.c#L245)
to that newly initialized histogram table, and [initialize its underlying word table](https://github.com/tpn/dictionary/blob/v0.7/Dictionary/AddWord.c#L278).

Once we've got a reference to the word table, [we can insert the word table entry for our given word](https://github.com/tpn/dictionary/blob/v0.7/Dictionary/AddWord.c#L305).

If we then add the word "below", the bitmap and histogram representation of the
word will be identical to those entries we just added for "elbow", which means
we'll end up at the same word table.  When we insert the word, the AVL mechanics
will determine that "below" is not equal to the existing node, "elbow", and
insert our new node into the tree.

Now, if we call [GetWordAnagrams](https://github.com/tpn/dictionary/blob/v0.7/Dictionary/Anagram.c#L22)
with the word "below", we'll first call out to the [FindWordTableEntry](https://github.com/tpn/dictionary/blob/v0.7/Dictionary/FindWord.c#L25)
routine, which not only returns the relevant word table entry for our given
word, but also fills out a special structure called [DICTIONARY_CONTEXT](https://github.com/tpn/dictionary/blob/v0.7/Dictionary/DictionaryPrivate.h#L593),
which we use to communicate various internal state between routines via TLS
(more on this later).

So, we also have access to our word table entry's parent WORD_TABLE via
[Context.WordTable](https://github.com/tpn/dictionary/blob/v0.7/Dictionary/Anagram.c#L170).

This will be the word table for all words that share our histogram.  Thus, we
can simply enumerate the table and build up a list of anagrams as we go.

## Tracking Lengths

It wasn't until the third day that I realized there was a subtle detail with
regards to the original anagram API specification:

    e.	struct dictionary_stats( void )
        i.	returns current longest word (ie. in dictionary)
        ii.	returns longest word ever (ie. in dictionary)

Returning the current longest word sounds simple enough until you consider what
happens when you remove the word that is currently the longest word?  Presumably
the next longest word in the dictionary will be seamlessly promoted to the new
current longest word.

So, we needed a way to efficiently track the lengths of all words in the
dictionary in a way that preserved ordering, such that upon removal of a word
that is also the current longest word of the dictionary, we need to find the
next longest length (which may be the same as our current length if other words
were added after us with the same length, or a lesser length, or no length at
all because this was the last entry in the dictionary).

So, to achieve that, we finally leverage a hash table.  Kidding, yet another AVL
table!  The dictionary has a [top-level LengthTable](https://github.com/tpn/dictionary/blob/v0.7/Dictionary/DictionaryPrivate.h#L535),
which is an instance of a [LENGTH_TABLE](https://github.com/tpn/dictionary/blob/v0.7/Dictionary/DictionaryPrivate.h#L185),
which captures LENGTH_TABLE_ENTRY records, which look like this:

```c
    typedef struct _LENGTH_TABLE {
        RTL_AVL_TABLE Avl;
    } LENGTH_TABLE;
    typedef LENGTH_TABLE *PLENGTH_TABLE;

    typedef struct _LENGTH_TABLE_ENTRY {
        LIST_ENTRY LengthListHead;
    } LENGTH_TABLE_ENTRY;
    typedef LENGTH_TABLE_ENTRY *PLENGTH_TABLE_ENTRY;
```

So, one of the roles of `AddWordEntry` is to [create a length table entry](https://github.com/tpn/dictionary/blob/v0.7/Dictionary/AddWord.c#L378)
for the given word's length if one doesn't already exist, and then
[link the word entry](https://github.com/tpn/dictionary/blob/v0.7/Dictionary/AddWord.c#L474)
to the linked list head.

We use the standard NT doubly-linked list LIST_ENTRY structures here because our
only requirement is to be able to detect if the [length list is empty](https://github.com/tpn/dictionary/blob/v0.7/Dictionary/RemoveWord.c#L270)
(which requires us to find the predecessor length table entry; i.e. the next
length less than ours), or if the list isn't empty,
[we can just use the next word entry in the linked list for this length](https://github.com/tpn/dictionary/blob/v0.7/Dictionary/RemoveWord.c#L424).

### Optimizations and Trade-offs

Bitmaps are 32 bytes and histograms are 1024 bytes.  If you have a million
entries in the tree and none of them are anagrams, that is nearly 1 GB of
overhead.  Do we have to store the entire bitmap and entire histogram in
every AVL table entry?  Not necessarily.  All we really need to be able to do
is provide an AVL comparison routine that can determine if two nodes are equal,
less than or greater than.  If we can generate a hash value for the underlying
bitmaps and histograms and words, we can just compare that instead of doing a
full-blown comparison.  If the hash doesn't match, then the nodes definitely
aren't equal.

So, one of the responsibilities of the [InitializeWord](https://github.com/tpn/dictionary/blob/master/Dictionary/Word.c)
routine is to generate hash values for the bitmap, histogram and word.
I use a single ULONG for the hash, and simply use CRC32 to generate it:

```c
    BitmapHash = Length;
    for (Index = 0; Index < ARRAYSIZE(Bitmap->Bits); Index++) {
        Hash.Index = Index;
        Hash.Value = Bitmap->Bits[Index];
        BitmapHash = _mm_crc32_u32(BitmapHash, Hash.AsULong);
    }
```

CRC32 is not a cryptographic hash, and even if it were, there is always the
possibility for collisions (two different values hashing to the same hash value)
when, for example, you're reducing a 1024 byte value into a 32-bit representation.

However, I figured CRC32 was a good enough starting point, so that's what I
used.  As long as it's deterministic that's all I really need (i.e. hashing the
same input will always produce the same output).

#### Handling Collisions

Collisions still need to be accounted for, though.  Two completely different
words with different histograms could end up hashing to the same histogram hash
value, which means they'll be lumped in the same word table, which up until now
has meant that the words are anagrams and should be reported as such.

I dealt with this in the [GetWordAnagrams](https://github.com/tpn/dictionary/blob/v0.7/Dictionary/Anagram.c#L277)
routine, which will enumerate all word entries in the given histogram table,
[recreate the full 1024 byte histogram](https://github.com/tpn/dictionary/blob/v0.7/Dictionary/Anagram.c#L315)
for each entry, and then compare that to the histogram of the input string using
[an AVX2-optimized histogram comparison routine](https://github.com/tpn/dictionary/blob/v0.7/Dictionary/Histogram.c#L26).
Only if that routine determines that the histograms match are the words
considered to be anagrams.

This trades extra CPU time for space savings, however the space savings are pretty
significant, and collisions should be quite rare with normal usage patterns.
CPUs are fast and histogram creation and comparison will be very cache-friendly
and predictable so overall it's a reasonable trade-off I think.

#### Stash The Hash

There's another reason I chose a ULONG to capture the hash value for everything.
If you look at the NT DDK, you'll see RTL_BALANCED_LINKS defined as follows:

```c
    typedef struct _RTL_BALANCED_LINKS {
        struct _RTL_BALANCED_LINKS *Parent;
        struct _RTL_BALANCED_LINKS *LeftChild;
        struct _RTL_BALANCED_LINKS *RightChild;
        CHAR Balance;
        UCHAR Reserved[3];
    } RTL_BALANCED_LINKS;
    typedef RTL_BALANCED_LINKS *PRTL_BALANCED_LINKS;
```

That structure is the header for every AVL table entry inserted into the table;
the Rtl insertion routines add in the size of the structure before calling the
allocator you've provided.  Your user data is then placed after that structure,
such that the actual structure looks something like this:

```c
    typedef struct _ENTRY_HEADER {
        RTL_BALANCED_LINKS Links;
        ULONGLONG UserData;
    } ENTRY_HEADER;
```

Now, what's interesting about that is that FIELD_OFFSET(ENTRY_HEADER, UserData)
returns 32, but if you calculate the size of RTL_BALANCED_LINKS by eyeballing
the elements, only 28 bytes are accounted for; 3 8-byte pointers and a ULONG.

What you don't see is the extra ULONG in padding that is implied due to x64
8-byte structure padding requirements.  This is where we stash our hash, such
that our [table entry header](https://github.com/tpn/dictionary/blob/v0.7/Dictionary/DictionaryPrivate.h#L260)
looks like this:

```c
    typedef struct _TABLE_ENTRY_HEADER {

        union {

            //
            // Inline RTL_BALANCED_LINKS structure and abuse the ULONG at the end
            // of the structure normally used for padding for our own purposes.
            //

            struct {

                struct _RTL_BALANCED_LINKS *Parent;
                struct _RTL_BALANCED_LINKS *LeftChild;
                struct _RTL_BALANCED_LINKS *RightChild;

                union {

                    struct {
                        CHAR Balance;
                        UCHAR Reserved[3];
                    };

                    struct {
                        ULONG BalanceBits:8;
                        ULONG ReservedBits:24;
                    };

                };

                //
                // For bitmaps, histograms and word table entries, we stash the
                // 32-bit CRC32 of the data in the following field.  For length
                // entries, the length is stored.
                //

                union {
                    ULONG Hash;
                    ULONG Value;
                    ULONG Length;
                };
            };

            RTL_BALANCED_LINKS BalancedLinks;

            //
            // Include RTL_SPLAY_LINKS which has the same pointer layout as the
            // start of RTL_BALANCED_LINKS, which allows us to use the predecessor
            // and successor Rtl functions.
            //

            RTL_SPLAY_LINKS SplayLinks;
        };

        //
        // The AVL routines will position our node-specific data at the offset
        // represented by this next field.  UserData essentially represents the
        // first 8 bytes of our custom table entry node data.  It is cast directly
        // to the various table entry subtypes (for bitmaps, histograms etc).
        //

        union {
            ULONGLONG UserData;
            struct _WORD_TABLE_ENTRY WordTableEntry;
            struct _LENGTH_TABLE_ENTRY LengthTableEntry;
            struct _BITMAP_TABLE_ENTRY BitmapTableEntry;
            struct _HISTOGRAM_TABLE_ENTRY HistogramTableEntry;
        };

    } TABLE_ENTRY_HEADER;
    typedef TABLE_ENTRY_HEADER *PTABLE_ENTRY_HEADER;
    C_ASSERT(FIELD_OFFSET(TABLE_ENTRY_HEADER, UserData) == 32);
```

It's a complete and utter abuse of structure padding, absolutely not recommended
by Microsoft, could cause everything to break at a later date if the AVL
implementation changes, and is a maintenance liability.

On the other hand, it's a free ULONG that actually saves us 8 bytes per entry,
not 4 bytes (because our table entry header would be subject to the same
alignment requirements and would be padded the same way), the RTL_AVL_TABLE and
RTL_BALANCED_LINKS structures and API methods have been publicly documented for
over a decade, Microsoft is excellent with regards to backwards compatibility;
if a new RTL_BALANCED_LINKS structure was necessary, it would be as part of a
new data structure (e.g. red/black trees) and would result in new structures and
API methods, leaving the AVL stuff unchanged.  (It's also trivial to change our
TABLE_ENTRY_HEADER definition to locate the Hash/Value/Length field elsewhere if
it does become problematic down the track.)

#### AVX2 Optimizations

AVX2 intrinsics are used in the [CompareHistogramsAlignedAvx2](https://github.com/tpn/dictionary/blob/v0.7/Dictionary/Histogram.c#L26)
and [CompareWords](https://github.com/tpn/dictionary/blob/v0.7/Dictionary/Word.c#L305)
routines.  Pros: faster.  Cons: will raise an illegal instruction exception if
the CPU doesn't have AVX2 support.

Additional optimizations could be made to the bitmap and histogram creation
logic using the pextr[bw]-type intrinsics.

## Misc Implementation Notes

1. Doesn't depend on the C runtime library.  The only dependency is on kernel32
   and ntoskrnl (and technically the Rtl.dll module but I just use that to help
   load the various Rtl routines I want).

2. The ALLOCATOR structure abstracts all calls to things like malloc/calloc.  If
   I were writing something that could have a 1GB memory presence let alone 1TB
   I would absolutely use memory-map backed files for all memory allocations,
   such that the on-disk representation and in-memory representation is one and
   the same.  The [tracer](https://github.com/tpn/tracer) project does this for
   trace stores, which work really nicely with Rtl splay/AVL routines.

3. Speaking of splay trees, I didn't use the RTL_GENERIC_TABLE structure and
   routines because they splay on every lookup, which is nice in some
   situations, but doesn't make sense for our bitmap/histogram/word table
   design, and would prevent using a slim R/W lock (because all readers mutate
   the table and would need an exclusive lock).

4. The design is thread-safe.  You can have multiple readers calling routines
   FindWord(), GetWordStats() and GetWordAnagrams() simultaneously.  AddWord()
   and RemoveWord() will implicitly acquire a write lock and block all readers
   (and other writers).

5. None of the internal data structure addresses (i.e. pointers) are exposed
   to the client.  If an API method requires memory to be allocated (like
   GetWordAnagrams()), the API call has an Allocator parameter which the user
   must provide.  The returned result must be freed by the caller when they
   are finished with it.  (Actually we don't care if they free it or not, it
   was their allocator, they're free to leak as much memory as they want.)

6. SAL annotations are provided for all functions.  Some of them are probably
   correct, too!  (I don't think I've ever used `_Outptr_result_nullonfailure_`
   correctly.)

7. Designed such that it could technically be run as a kernel module (assuming
   the relevant legacy driver/IOCTL scaffolding).  There would need to be a
   kernel-specific ALLOCATOR interface that writes into a buffer that the user
   provided but other than that it should be fine; one of the perks of only
   relying on ntoskrnl methods.


# Journal

## Day 1

1. Set up initial project scaffolding.  Import a bunch of things from the
   Tracer project that I envision probably using.  At the very least, the
   Rtl component will get used heavily as it provides a convenient access
   point for all the NT kernel functions I want to use (e.g. for bitmaps
   and AVL tables).  If time permits I can use the trace store component
   to efficiently test large dictionary representations.  Otherwise I'll
   do a cleanup toward the end and remove all the stuff that's not being
   used directly.

2. Flush out some very initial Dictionary component elements.  The anagram
   requirement is currently dictating the design.  I anticipate using a
   character frequency histogram in an AVL tree as the main data structure,
   allowing for word entries to be linked based on identical anagrams at
   dictionary entry time.

3. Implement [CreateCharacterBitmapForStringInline](https://github.com/tpn/dictionary/tree/v0.1/Dictionary/Dictionary.h#L205),
   [CreateCharacterFrequencyHistogramForStringInline](https://github.com/tpn/dictionary/blob/v0.1/Dictionary/Dictionary.h#L286)
   and [CreateAndInitializeDictionary](https://github.com/tpn/dictionary/blob/v0.1/Dictionary/Dictionary.c#L5).

4. Whip up a little scratchpad helper, [ScratchExe](https://github.com/tpn/dictionary/blob/v0.1.1/ScratchExe/main.c#L19),
   that exercises the functions above.  Once things are flushed out
   more I'll probably switch to unit tests via the TestDirectory
   component.

Hours: 7.38

## Day 2

1. Implement the initial unit test scaffolding and convert yesterday's bitmap
   and histogram tests into corresponding unit tests.

2. Add a CRC-32 64-bit hash routine for the histogram in additional to the
   32-bit one.

3. Flush out the initial bitmap and histogram table and table entry structures.

4. Flush out more of the underlying dictionary structure.

5. Fix some build issues and do some cleanup of various items copied over
   yesterday.

6. Add some TLS glue that'll facilitate structure optimization regarding table
   entries.

7. Finish the initial CreateAndInitializeDictionary() implementation

8. Stub out the initial add word/find word etc functions ready for tomorrow.

Hours: 7.17 (3.07, 1.33, 2.77)

## Day 3

1. Revamp public/private split of interface.
2. Flush out main body of AddWord().
3. Move the bitmap and histogram functions into a single InitializeWord()
   function.
4. Extend CreateDictionary() and flush out initial DestroyDictionary().
5. Update scratch.exe and unit tests.
6. Introduce single 'Tables.c' module that contains the AVL-specific glue.

Sent Omar a query regarding two things:

a) Should a word's maximum count persist past the word's complete removal from
the directory?

b) Confirm that if the current maximum length word is deleted, the dictionary is
expected to update its internal state such that the next longest word length is
promoted to longest.

The latter is a subtle detail I only caught today.  I'm implementing the
functionality via (yet another) AVL table; one specifically for word lengths.

Hours: 11.08 (2.6, 3.27, 2.05, 3.17)

## Day 4

Notable design decision change: decided to *not* store bitmaps or histograms in
any of the AVL trees, simply relying on the 32-bit hashes we embed within the
AVL table entry header nodes for determining equality.

This saves about 128 bytes per unique bitmap and over 1024 bytes per unique
histogram, which is substantial.

Hash collisions will be possible, however, this will really only affect the
anagram functionality, in that we'll need to traverse a given histogram table
when we've been asked to find all anagrams for a given word.

The histogram comparison routine [CompareHistogramsAlignedAvx2](https://github.com/tpn/dictionary/blob/v0.4/Dictionary/Histogram.c#L24)
has been implemented using AVX2 intrinsics as the name would suggest, which
will improve the performance of the histogram comparisons when identifying
anagrams.  I anticipate re-creating the histogram for each string as part
of the anagram logic -- so we're paying some CPU costs for sizable space
savings.

Word lookup should be fast in the case where the underlying string hashes differ.
In the case where the string hashes are identical, another AVX2-optimized string
comparison routine was written to improve the performance of this operation:
[CompareWords](https://github.com/tpn/dictionary/blob/v0.4/Dictionary/Word.c#L231).

This routine compares up to 32 bytes at a time using non-temporal streaming loads if
possible.  It takes into account underlying string buffer alignment and
potential issues when crossing page boundaries.

The AddWord() routine is mostly finished and works, it just needs to have some
logic added to it to deal with registering as the longest string if applicable.

Hours: 9.67 (3.03, 5.62, 1.02)

## Day 5

[Main commit of the day's
progress](https://github.com/tpn/dictionary/commit/013bdbe7c10f8e5f81060831e9934f21d9997a09)
was quite sizable.  First pass at the
[Anagram](https://github.com/tpn/dictionary/blob/v0.5/Dictionary/Anagram.c)
module, which implements the `GetWordAnagrams()` function.

Also implemented the word finding functionality via the
[FindWord](https://github.com/tpn/dictionary/blob/v0.5/Dictionary/FindWord.c)
module.

No major design decision changes, two minor ones:

1. Decided to commit to hiding the WORD_ENTRY implementation details and only
   exporting public functions that work against the PCBYTE arrays (i.e.
   `unsigned char *`'s representing the words).  This means none of the public
   functions ever see the addresses of data structures allocated internally
   by the diciontary; if they want to obtain values that require memory
   allocation, the function now requires an Allocator parameter, and all
   user memory required to return results is done via that.

   This was done for two primary reasons: a) returning internal addresses
   wouldn't work if this was a kernel module; data would need to be transferred
   into a buffer provided by the user, and b) it defeated the thread-safe
   locking provided by the dictionary when used via the public API.

2. Continued to abuse spare fields in the Rtl structures, this time leveraging
   two unused ULONGs in the RTL_AVL_TABLE layout to track the total number of
   bytes allocated by a given word table.  This is leveraged by the anagram
   logic to optimize how we determine the amount of memory to allocate to
   contain the anagram list.  (We want to perform a single allocation call so
   that the user can eventually free the resulting pointer with a single free
   call.)


Other than that everything appears to be behaving quite nicely and the original
design decisions have held up nicely (bitmap -> histogram -> word table).

Last major piece remaining is implementing RemoveWord().  This will be a little
fiddly due to the requirement to track current and all-time longest lengths of
words at the dictionary level.

Hours: 11.24 (3.4, 3.57, 3.05, 1.22).

## Day 6

Implement
[RemoveWord](https://github.com/tpn/dictionary/blob/v0.6/Dictionary/RemoveWord.c#L20)
and some initial supporting
[tests](https://github.com/tpn/dictionary/blob/v0.6/TestDictionary/unittest1.cpp#L678).

It was about as fiddly as I anticipated due to the longest length tracking
requirement but otherwise no issues.  The length promotion approach worked
as anticipated.

Added bonus: freed up a pointer per word table entry; we don't need to track the
length table entry as we can get the next longest length from some linked-list +
CONTAINING_RECORD() manipulation.

Only function that remains unimplemented is GetWordStats() but that won't take
long given that everything else is in place.

More tests are warranted for all the various behavioral permutations, I'll add
some more tomorrow.

The current state of the repo represents the first point everything is basically
implemented and tested and working... remaining changes will just be bug fixes
and cleanup.

Edit: implemented GetWordStats(), so everything is basically finished.

Hours: 9.0 (2.28, 6.72)

## Day 7

Updates to documentation; comments, docstrings and this README.

<!-- vim:set ts=8 sw=4 sts=4 tw=80 expandtab                              :  -->

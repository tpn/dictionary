# Dictionary

A dictionary component with anagram support.

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
   [CreateCharacterFrequencyHistogramForStringInline](https://github.com/tpn/dictionary/blob/master/Dictionary/Dictionary.h#L286)
   and [CreateAndInitializeDictionary](https://github.com/tpn/dictionary/blob/v0.1/Dictionary/Dictionary.c#L5).

4. Whip up a little scratchpad helper, [ScratchExe](https://github.com/tpn/dictionary/blob/v0.1.1/ScratchExe/main.c#L19),
   that exercises the the functions above.  Once things are flushed out
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

<!-- vim:set ts=8 sw=4 sts=4 tw=80 expandtab                              :  -->

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


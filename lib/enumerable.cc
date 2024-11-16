import apple

walking    : enum [ 2 ]  # allowed, this makes incrementing in design by default, first is 0 if no enum was set
car        : enum [ ]
bus        : enum [ ]
shuttle    : enum [ ]
van        : enum [ ]
space-ship : enum [ ]
hovercraft : enum [ ]
pogostick  : enum [ ]

void print-something[ bool nice ] [
    if nice print 'hi' else print '????'
    print 'since this is a local function, we may operate on the data of our module in local context'
]


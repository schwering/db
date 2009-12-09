# Counts the tuples in the relation that's the result of `query'.
# Of course, iterating through the complete relation is not very efficient 
# in many cases (e.g. if an index exists).
PROCEDURE COUNT(STRING query)
BEGIN
	INT count;
	TUPLE t;
	INT pos;
	STRING sub;

	# check whether query is `;'-terminated
	pos := (!strlen(query) - 1);
	sub := !substr(query, pos, 1);
	IF ((sub != ";"))
		query := (query + ";");

	count := 0;
	FOREACH (t IN query)
		count := (count + 1);
	RETURN count;
END


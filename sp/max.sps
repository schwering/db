# Determines the maximum value in `attr' in the relation that's the result of 
# `query'.
# Of course, iterating through the complete relation is not very efficient 
# in many cases (e.g. if `attr' is indexed).
PROCEDURE MAX(STRING query, AUTO attr)
BEGIN
	TUPLE t;
	INT pos;
	STRING sub;
	AUTO cur;
	AUTO max;
	INT init; # ensures that max is set at least once

	# check whether query is `;'-terminated
	pos := (!strlen(query) - 1);
	sub := !substr(query, pos, 1);
	IF ((sub != ";"))
		query := (query + ";");

	max := 0;
	init := 0;
	FOREACH (t IN query) DO
		cur := !attrval(t, attr);
		IF ((init = 0)) DO
			init := 1;
			max := cur;
		END
		IF ((cur > max))
			max := cur;
	END

	RETURN max;
END


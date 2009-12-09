# Calculates the average value of the result of `query' in `attr'.
# The result has the type FLOAT.
PROCEDURE AVG(STRING query, STRING attr)
BEGIN
	TUPLE t;
	INT pos;
	STRING sub;
	FLOAT avg;
	FLOAT cur;
	FLOAT cnt;

	# check whether query is `;'-terminated
	pos := (!strlen(query) - 1);
	sub := !substr(query, pos, 1);
	IF ((sub != ";"))
		query := (query + ";");

	avg := 0.0;
	cnt := 0.0;
	FOREACH (t IN query) DO
		cur := !to_float(!attrval(t, attr));
		avg := ( ((cnt / (cnt+1)) * avg) + (cur / (cnt+1)) );
		cnt := (cnt + 1);
	END
	RETURN avg;
END


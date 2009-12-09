PROCEDURE CreateDBs(STRING name, INT size)
BEGIN
	STRING query;
	INT i;

	query := (("DROP TABLE " + name) + ";");
	!exec(query);

	query := ("CREATE TABLE " + (name 
		+ " (i INT PRIMARY KEY, s STRING(64), t STRING(64));"));
	IF ((!exec(query) = 0))
		RETURN (-2);

	query := ("CREATE INDEX ON " + (name + " (s);"));
	IF ((!exec(query) = 0))
		RETURN (-1);

	i := 0;
	WHILE ((i < size)) DO
		query := ("INSERT INTO "+ name);
		query := (query + " (");
		query := (query + name);
		query := (query + ".i, ");
		query := (query + name);
		query := (query + ".s, ");
		query := (query + name);
		query := (query + ".t) VALUES (");
		query := (query + i);
		query := (query + ", '");
		query := (query + i);
		query := (query + "', '");
		query := (query + i);
		query := (query + "');");
		IF ((!exec(query) = 0))
			RETURN i;
		i := (i + 1);
	END
	RETURN 0;
END


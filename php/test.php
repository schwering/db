<?php
if (!empty($_GET["src"])) {
	highlight_file("test.php");
	die();
}

$query = stripslashes(trim($_GET["q"]));
if (empty($query)) {
	$query = "SORT ( 
	PROJECT ( 
		JOIN
			(JOIN people, places),
			salaries 
	) OVER people.name,
	     people.descr,
	     places.place,
	     salaries.age,
	     salaries.salary 
) BY people.name;";
}

function query($q)
{
	$order = array("\r\n", "\n", "\r");
	$replace = "";
	$q = str_replace($order, $replace, $q);

	$r = db_parse($q);
	
	if (!db_success($r)) {
		echo "Database error when executing: <pre>'$q'</pre>";
	}
	
	if (db_is_definition($r)) {
		echo "Definition was successful";
	}
	
	if (db_is_modification($r)) {
		echo "Modification was successful";
	}
	
	if (db_is_sp($r)) {
		echo "Calculated value is ". db_spvalue($r) .".";
	}
	
	if (db_is_query($r)) {
		$iter = db_iterator($r);
		$header = false;
		echo "<table border='1'>";
		while (($row = db_next($iter, true))) {
			if (!$header) {
				$attrs = array_keys($row);
				echo "<tr>";
				foreach ($attrs as $attr)
					echo "<th>$attr</th>";
				echo "</tr>";
				echo "\n";
				$header = true;
			}
			echo "<tr>";
			foreach ($row as $val)
				echo "<td>$val</td>";
			echo "</tr>";
			echo "\n";
		}
		echo "</table>";
		db_free_iterator($iter);
	}

	db_free_result($r);
}
?>

<html>
<body>
<div align="right"><a href="test.php?src=1">PHP-Kot angaffen</a></div>
<form method="get" action="test.php">
<textarea type="text" name="q" rows="10" cols="80" />
<?php echo $query; ?>
</textarea><br />
<input type="submit"><br />


<?php
if (empty($query))
	exit();

if ($query[strlen($query)-1] != ';')
	$query .= ";";

$queries = split(";", $query);
foreach ($queries as $q) {
	if (strlen($q) == 0)
		continue;
	echo "<h2>$q</h2>";
	query($q .";");
	echo "<hr />";
}

db_cleanup();
?>
</body>
</html>

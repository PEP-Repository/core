const static std::string BEGIN_GROUP_SELECTION_TEMPLATE = R"RAWSTRING(
<!DOCTYPE html>
<html lang="en">
<head>
<title>Select group</title>
<style>
html body {
  background: white;
  display: block;
  margin: 0 auto;
  margin-bottom: 0.5cm;
  font-family: helvetica;
  text-align: center
}

div selectGroup{
  font-family: helvetica;
  font-size: xx-large;
}

div groupForm {
  font-family: helvetica;
  font-size: large;
}
</style>
</head>
<body>
  <form method="POST">
    <label for="user_group">You are in multiple groups. Please select the one you want to login for.</label><br>
    <select id="user_group" name="user_group" required>
      <option value="">Please select...</option>
)RAWSTRING";

const static std::string END_GROUP_SELECTION_TEMPLATE = R"RAWSTRING(
    </select>
    <input type="submit">
  </form>
</body>
</html>
)RAWSTRING";
# PEP Tutorial

## General approach and goals

In this tutorial you will learn the basics of PEP. Step by step, we will guide you through the system. After completing this tutorial you will have a basic understanding of:

- Logging in
- Down- and uploading data
- Repository management
- The privacy and security principles behind PEP

!!! warning "Command Line"
    In this tutorial you will be using the PEP command line interface (pepcli). Some familiarity with command line interfaces is recommended, but not strictly required, as we will explain all commands and steps to take.

!!! note "Terminology Transition"
    Our terminology was historically biased towards academic research in a medical context. We are now transitioning to more field-neutral terminology to better reflect PEP's current scope. For example, "Participant Groups" are now called "Subject Groups". Throughout this tutorial, we will use the new terminology, but you may still encounter the older terms in our command line interface or documentation. Both refer to the same concepts. See [Glossary](../glossary/#legacy-old-terminology) for more details.

For any questions, please contact <support@pep.cs.ru.nl>.

## Downloading the software and starting

### Prerequisites

For this tutorial, you need Docker installed on your system:

- **Windows/Mac**: Download and install [Docker Desktop](https://www.docker.com/products/docker-desktop).
- **Linux**: Install Docker Engine via your package manager.

### Starting the PEP Tutorial Environment

The PEP tutorial is a self-contained environment that runs in a Docker container. It includes all PEP servers (which normally run in a distributed architecture), the command line interface to interact with the system (pepcli), and a pre-populated repository with example data. Note that the code blocks in this tutorial have a clipboard icon in the top right corner. You can click this icon to copy the command, making it easier to paste into your terminal. To start the tutorial environment, follow these steps:

1. Open a terminal/command prompt and run:

    ```shell
    docker run -d --rm --name pep-sandbox gitlabregistry.pep.cs.ru.nl/pep-public/core/pep-sandbox:latest && docker logs -f pep-sandbox
    ```

!!! note "Note"
    Note that if you use MacOS with Apple Silicon, you may need to run the above command with the extra flag `--platform linux/amd64` to emulate the correct CPU architecture, like this:

    ```shell
    docker run -d --rm --name pep-sandbox --platform linux/amd64 gitlabregistry.pep.cs.ru.nl/pep-public/core/pep-sandbox:latest && docker logs -f pep-sandbox
    ```

  This will start the PEP tutorial environment and show you the setup logs. Wait until you see the message:

  ```plaintext
  ================================================================
  PEP Tutorial Sandbox ready!
  ================================================================
  ```

2. Press ++ctrl+c++ to stop following the logs (the container keeps running in the background).

3. Enter the container by running:

    ```shell
    docker exec -it pep-sandbox bash
    ```

You are now inside the PEP tutorial environment. We will refer to this as *the terminal*.

### Disclaimers

!!! warning "Simplified setup"
    Please note that this is a simplified setup for demonstration purposes of a small subset of the features of the command line interface. Do not use any parts of the pep-sandbox setup for any other purpose than demonstration, as it has no persistent storage and is not secure.

!!! warning "pepcli output"
    When using pepcli (explained later), the terminal might show: `2026-02-12 15:53:49: <warning> [Networking client] Could not establish connection for client to tls://Authserver:16512: boost::system::system_error: Connection refused [system:111]. Retrying in 1000ms`. This is due to the way the tutorial environment is set up, and can be safely ignored.

## Logging in

In the terminal we can interact with the PEP repository. To log in, type `pepLogon` into your terminal and hit Enter. This will show you a menu of available **User Groups**:

```plaintext
PEP Tutorial, select User Group:
==========================
1) Research Assessor
2) Data Administrator
3) Access Administrator

Enter your choice (1-3):
```

We have a number of predefined **User Groups** in the PEP repository, such as **Research Assessor**, **Data Administrator**, and **Access Administrator**. More on these **User Groups** [later](#intermezzo-four-eyes-principle).

For now, we will use the **Research Assessor** **User Group**. Enter the number corresponding to this group and press ++enter++.

You are now logged in!

!!! note "Note"
    This tutorial demonstrates workflow for several different groups of users, in order to show overarching concepts. In a real life scenario, these would be different people, but for the sake of demonstration, you will act as all of these users in this tutorial.

## Looking around

Now that we are logged in, we can have a first look around in the PEP repository. For this tutorial, we have already prepared some data, considering the students `Alice`, `Bob`, `Charles`, and `Danielle`. First, let's download the data for these subjects with the following command:

```shell
pepcli pull --all-accessible --export csv
```

After the process has finished, you should have a (new) folder called `pulled-data` in your current working directory. Open it and look around:

```shell
ls pulled-data
```

You will see that we have four subfolders with the prefix `BLPS` and then a number of hexadecimal characters (like `BLPS24680954B6BEB3C3`), each subfolder representing a different **Subject**. The names of these subfolders are **Brief Local Pseudonyms**, shortened versions of the full [**Local Pseudonyms**](#intermezzo-polymorphic-pseudonymisation), made easier to work with by adding a recognizable prefix and truncating the full identifier. Take a look inside the subfolders:

```shell
ls pulled-data/<PSEUDONYM>
```

Don't forget to change the `<PSEUDONYM>` to an actual pseudonym you see as folder name. This will show a number of files, each corresponding to a column in the repository:

```shell
Address.txt  Class.txt  French_Grade.txt  French_Level.txt  Maths_Grade.txt  Maths_Level.txt  Name.txt  ParticipantIdentifier.txt
```

You should see a file called `Name.txt`, which contains the name of the **Subject**. Print the contents of that file to the terminal:

```shell
cat pulled-data/<PSEUDONYM>/Name.txt; echo
```

Because we added the `--export csv` parameter to the pull command earlier, a file called `export.csv` was also created. This file contains all data for your query in a single `.csv` file. Check out its contents:

```shell
cat export.csv
```

The output should be similar to the following table:

| id | Maths_Grade | French_Grade | Class | Maths_Level | Name | French_Level | Address | ParticipantIdentifier |
| - | - | - | - | - | - | - | - | - |
| 1805012D... | 6 | 7 | 2A | D | Charles | C | Lange Laan 3 | OID707357994245 |
| F8E5B43D... | 3 | 8 | 2A | A | Alice | A | Fruitstraat 12 | OID438522405587 |
| 362E48DF... | 7 | 4 | 3B | C | Bob | A | Lange Laan 3 | OID322226314994 |
| 541564BC... | 8 | 5 | 1C | A | Danielle | B | Appelstraat 7 | OID971997288116 |

The table visualises the points in `rows` (the subjects `Alice`, `Bob`, `Charles` and `Danielle`), and `columns` (`Name`, `Address`, `Class`, etc.).

The `id` column shows **Local Pseudonyms**, unique identifiers for subjects that are specific to your **User Group**. These are quite long (64 hexadecimal characters), which is why PEP also generates **Brief Local Pseudonyms** (like `BLPS24680954B6BEB3C3` in the folder names) by adding a prefix and truncating them for easier handling. Finally **Origin IDs** are shown in the `ParticipantIdentifier` column. These are random identifiers generated by the system when you register a new subject, but more on that later.

Imagine you're an analyst studying student achievement. This data would be useful, but also contains a lot of information that is not required. You have *no need* for the actual names of the students, let alone the address where the students live. As long as you can consistently identify single subjects, you have enough information to perform your analysis.

!!! example "Intermezzo: Privacy by design"
    {: #intermezzo-privacy-by-design}
    The ground rule for working with personal data (and all data in the table above is personal data, not just the names and the addresses) is that the person working with these data should only have access to the data that is required for the purpose. This concept is called *data minimisation*.

    Another important aspect to keep in mind is *proportionality* and *purpose limitation*, for example: it might be valuable to register mental conditions of subjects when performing a study, but on the other hand this imposes a much larger impact on the privacy of an individual than for example sharing a math grade. Although efforts are made to prevent identification of individuals, this can never be completely prevented. Therefore the proportionality of using such details should be considered, and in the case of very sensitive data, (even) stronger ethical checks should be performed concerning the consent basis and the taken privacy and security measures. But the bottom line about data sharing is: less is more, which concerns both the amount of data and the resolution and sensitivity of data. Instead of, for example, sharing a birth date (day), in many cases the year of birth may suffice. In those cases, only this year of birth (a lower resolution) should be shared/used.

## Using a different scope

Let's see what happens when a different **User Group** accesses the data. Log in as **Data Administrator**:

```shell
pepLogon
```

Select **Data Administrator** from the menu, then pull all accessible data, while exporting to `.csv` and forcefully overwriting the old `export.csv`:

```shell
pepcli pull --all-accessible --export csv --force
```

Check the exported `.csv` file:

```shell
cat export.csv
```

You'll see output like this:

| id | Maths_Level | French_Grade | French_Level | Maths_Grade | Class |
| - | - | - | - | - | - |
| 04FC65AE... | A | 8 | A | 3 | 2A |
| C0A53521... | A | 5 | B | 8 | 1C |
| 945CC6EF... | D | 7 | C | 6 | 2A |
| 26B49563... | C | 4 | A | 7 | 3B |

Notice two key differences from what the **Research Assessor** could see:

1. No personal information: this **User Group** is configured to see no personal information: The **Data Administrator** cannot see `Name` or `Address` columns. They have no need for this information, in this case, their role is to manage the repository structure, not to work with identifying details.

2. Different **Local Pseudonyms**: compare these pseudonyms (`04FC65AE...`, `C0A53521...`) to the ones you saw as **Research Assessor** (`1805012D...`, `F8E5B43D...`). These pseudonyms are completely completely different from the ones you saw before! In fact, only this **User Group** will have access to these pseudonyms, and can use them to identify subjects available to their **User Group**, not even the PEP servers will know them. Every **User Group** has consistent pseudonyms within the group, but different **User Groups** see different pseudonyms for the same subjects. This makes sharing data and coupling different parts of the dataset outside PEP a lot harder.

Performing the same download as **Access Administrator** will show you that this **User Group** cannot see any data at all, not even pseudonyms. This is because the **Access Administrator** is only responsible for managing access control, and has no need to see any data.

This demonstrates how PEP enforces *data minimisation* through access control: each **User Group** sees only what's necessary for their purpose, and even the identifiers differ between **User Groups** to prevent unauthorized data linkage.

!!! example "Intermezzo: Polymorphic pseudonymisation"
    {: #intermezzo-polymorphic-pseudonymisation}
    The fact that every **User Group** has their own pseudonyms is a very important concept within PEP. So much so, that it is half of the namesake of the system, **Polymorphic Encryption and Pseudonymisation**. PEP uses multiple types of pseudonyms for different purposes:

    1. **Origin IDs**: The original identifiers for a subject in the system, you saw this before as `ParticipantIdentifier` when downloading. This identifier can be randomly generated when registering a new subject, or could be derived from an existing one (e.g. a student number, or a BSN). **Origin IDs** are never shared with users, they are only used internally for data management and encryption, in this case by the **Research Assessor** who registers the subjects and generates the Origin IDs for them.
    2. **Polymorphic Pseudonyms**: El-Gamal encrypted **Origin IDs**, which are randomized and rekeyed. Due to this prodecure, these pseudonyms are not fixed, ensuring they cannot be compared to link data.
    3. **Local Pseudonyms**: Derived from polymorphic pseudonyms, these are fixed identifiers for a specific **User Group**. Every **User Group** sees different local pseudonyms for the same subjects. Abbreviated versions are also available for easier handling, which you saw as **Brief Local Pseudonyms** in the folder names.
    4. **Reference IDs**: Fixed identifiers generated for specific external use cases, such as linking physical specimens, survey responses, or file uploads to subjects in PEP, more one these [in the bonus section](#bonus-using-reference-ids-for-privacy-preserving-data-upload).

    Imagine two research groups each receive a subset of data:

    - Group A downloads math grades with pseudonyms like `BLPS111`, `BLPS222`, `BLPS333`
    - Group B downloads french grades with pseudonyms like `BLPS444`, `BLPS555`, `BLPS666`

    Even if both groups share their data publicly or the data leaks, *no one can determine which math grades belong to which french grades* because the pseudonyms are completely different. Combining datasets creates a more complete profile of individuals, increasing privacy risks. Math grades alone reveal little, but combining math grades, french grades, address, absentee data etc. builds a profile and could enable re-identification.

    Within PEP, the only way to link data across **User Groups** is through the system itself, under controlled authorisation.

## Registering new subjects and uploading data

So far we've only worked with existing data. Now let's see how to add new subjects and upload data for them. Before storing any information about a new student, we must first register them in the system. Let's log in again as the **Research Assessor** user:

```shell
pepLogon
```

We can then use the following command to register a new **Subject** by generating an **Origin ID**:

```shell
pepcli register id
```

Make sure to write down the identifier (e.g. `OID861383034282`). We will use this identifier to store data in the columns we saw above.

### Storing a value from the terminal prompt

We start by storing the name of this new subject. Using the `--data` parameter we can add the data directly from the terminal.

!!! tip "Tip"
    If you want to store values with spaces in it, surround the value with double quotes, e.g.: `--data "Henk de Vries"`.

Make up a new student name and store it with the following command:

```shell
pepcli store --participant <IDENTIFIER> --column Name --data <SUBJECTS_NAME>
```

where `<IDENTIFIER>` should be replaced with the identifier you have just written down, and `<SUBJECTS_NAME>` should be replaced by the name you made up. Note that `pepcli` still uses the older term `participants` instead of **Subjects** due to our ongoing terminology transition.

This should produce an output on your terminal screen similar to:

```shell
2026-02-13 14:09:45: <info> [Application binary] pep/core 1.7.6957.778661 - 02f66d493044150cfadb (Linux)
2026-02-13 14:09:45: <info> [Application configuration] gum:main 1.7.6957.778661
2026-02-13 14:09:45: <warning> [Networking client] Could not establish connection for client to tls://Authserver:16512: boost::system::system_error: Connection refused [system:111]. Retrying in 1000ms
{
    "id": "0A52310F0FAD016ED8D4A0C304D4EBB85D1FF5E0DABE8D87E8EBD31CD2960504E105B51231E87EB36A41C9C1652B8BDBA4E87979EDE2EAB3DDC9384DD9F2FD706C4ED55D53940069E1028D290E13370883E804DF12105DA9A1C745E6373AF913E7ABC5C5A6AC1A10CFDA4D428E8BFDF4B823BB427C8CBD73"
}
```

#### Storing a value from a file

It is also possible to upload files, using the `--input-path` parameter and adding the path to the file. Let's first make a file called `participantAddress.txt`:

```shell
echo "Rainbow Road 123" > participantAddress.txt
```

You can use whichever address you like, instead of `Rainbow Road 123`. Then, we can use the following command to store the address for this new subject:

```shell
pepcli store --participant <IDENTIFIER> --column Address --input-path participantAddress.txt
```

- Use one of the above methods to upload the `Class` for the new subject, using `--column Class`. Make sure they are in the second class! This will be important in a bit.
- Use the one of the commands above to fill the `Maths_Grade` column.
- (Optional) Fill in the remaining columns (`Maths_Level`, `French_Level`, `French_Grade`).

!!! example "Intermezzo: Four Eyes principle"
    {: #intermezzo-four-eyes-principle}
    The administrative superpowers in PEP are divided over two specific types of predefined **User Groups**. One is the **Data Administrator** and the other is the **Access Administrator**. Both administrators work together and need each other to administer the PEP repository. This separation is called the **four-eyes principle**. The reason for this separation of tasks is that a potential breach of one of the administrator **User Groups** will not immediately lead to a data leak. In this tutorial, you will act as both **User Groups** although this should never be the case in operational environments.

    The **Data Administrator** is intended for repository managers, they create columns, organising them into **Column Groups**. They also add **Subjects** to **Subject Groups**. They typically work with pseudonymised data and need only read access to non-personal information. When temporary access to personal data is required for organisational tasks (such as grouping subjects by characteristics), such access should be granted for a limited time by the **Access Administrator**.

    The **Access Administrator** is intended for people who manage the access control of the repository. They do not need access to any data, but they can:

    - Create **User Groups**.
    - Add users.
    - Grant users access to **User Groups**.
    - Can grant **User Groups** access to **Column Groups**, and **Subject Groups**.

    A final predefined **User Group** is the **Research Assessor**. This group is intended for people who have direct contact with the subjects, such as those handling enrollment or coordinating data collection. Because they interact directly with subjects, they require read and write access to personal data including names and addresses.

## Demonstrating selective data access

In previous examples more details on the subjects were shared than required. In the following part, we'll demonstrate one of PEP's key features: *different User Groups can access different subsets of data*. Instead of giving downloaders access to all data, we can specify access exclusively to specific columns and subjects for all members in a **User Group**. This is done by grouping columns and subjects in **Column Groups** and **Subject Groups**.

 We'll create:

1. A **Column Group** for grades.
2. A **Subject Group** containing second-year students.
3. A new `Analyst` **User Group** with access to the grades of second-year students only.

This demonstrates how PEP enables fine-grained access control, allowing different research groups to access only the data relevant to their specific research questions.

### Creating a new Column Group

Make sure you are logged in as **Data Administrator** by using the `pepLogon` command:

```shell
pepLogon
```

And choose `Data Administrator` from the menu.

Now, as **Data Administrator**, use the following command to create a new **Column Group**:

```shell
pepcli ama columnGroup create Grades
```

Then, add the requested columns to the **Column Group**:

```shell
pepcli ama column addTo Maths_Grade Grades
pepcli ama column addTo French_Grade Grades
pepcli ama column addTo History_Grade Grades
```

We'll also need a column where analysts can upload their findings, so let's make a column named `Average`:

```shell
pepcli ama column create Average
```

and add it to a new **Column Group** with the same name:

```shell
pepcli ama columnGroup create Average
pepcli ama column addTo Average Average
```

### Creating the Subject Group

The analysts are interested in the grades of second year students, so let's make a **Subject Group** containing only those subjects:

```shell
pepcli ama group create SecondYearStudents
```

The data administrator needs to know which subjects have to be in that **Subject Group**. To do this, we should retrieve every subject's class and pseudonym and add the subject when the class is "2A", "2B", etc. Remember that we just created a new subject, who is not yet in any **Subject Groups**. To find them, we have a special **Subject Group**: `*`. The `*` **Subject Group** contains ALL subjects in the repository. Access to this **Subject Group** is very limited, and only used sparingly.

Because we are dealing with a small set of data that we do not necessarily want to keep, instead of the `pull` command, we will use the `list` command. This will not write the data to files, but show them in the terminal.

Use the pepcli list command to show the `Class` column for all subjects in the repository, using the `*` **Subject Group**. We also add the `--local-pseudonyms` parameter to show the **Local Pseudonyms** in the output, which are a bit easier to copy than the **Polymorphic Pseudonyms**:

```shell
pepcli list --participant-groups "*" --columns Class --local-pseudonyms
```

The result will be a list with items similar to the following, one for each subject in the repository available to you:

```json
{
  "data": {
    "Class": "2A"
    },
  "pp": "AEB4D393CE1847968A36BBC199A69D58A19C5B1072EC99C1F8771CE1F133C831:90E897D30D2AF7EB4781D83E9CCD6A0B7603531CCD909AEA281454FD941C9800:CAB97D7E594B0DCA79BDFBDB090B08C153E761CF964CDA32D829BB746934B34B",
  "lp": "721A7A40DF4D1D2684F522E34E9329816026B04FD5518779F6D528898F5D7F43"
}
```

where the `Class` value is shown in the `data` section, the **Polymorphic Pseudonym** is shown in the `pp` field, and the **Local Pseudonym** is shown in the `lp` field.

- Look for subjects that are in their second year, copy the `lp` value, and use the following command to add them to our new **Subject Group**.

```shell
pepcli ama group addTo SecondYearStudents <FOUND_IDENTIFIER>
```

Don't forget to replace `<FOUND_IDENTIFIER>` with the `lp` you have selected. Repeat this step for all second year students.

!!! example "Intermezzo: Encryption"
    {: #intermezzo-encryption}
    **End-to-end encryption** {#end-to-end-encryption}

    The process just used is rather tedious. It would be nice if PEP supported data driven processes such as the one above (e.g., "add all subjects where `Class` starts with '2' to the `SecondYearStudents` group"). However, there is a very good reason we do not. PEP is *end-to-end encrypted*, another principle of Privacy by Design, this means:

    - Data is encrypted on your device before being sent to the PEP servers
    - The PEP servers store only encrypted data, they cannot read it
    - Data is decrypted on your device when you download it
    - Even if someone gains access to the PEP servers, they cannot read the data

    The whole time data is in the PEP repository, it is encrypted, meaning that the PEP servers can only perform very limited logic on it. Things as automating **Subject Group** assignment based on data contents is by choice impossible to do. Logic like this will have to be performed or scripted by the **Data Administrator** after retrieving the data in an unencrypted form.

    This is fundamentally different from typical database systems where the server can read all the data it stores.

    **Polymorphic encryption** {#polymorphic-encryption}

    While PEP provides end-to-end encryption like many other systems, it adds an important layer of protection through *trust distribution*. In typical end-to-end encrypted systems, you must trust the server operator not to:

    - Modify the server software to capture encryption keys.
    - Be compelled by legal orders to provide access.
    - Have their server compromised by attackers.

    PEP addresses this through **polymorphic encryption**, a cryptographic technique that distributes trust across multiple independent servers. These servers can be operated by different organisations, even in different legal jurisdictions or continents. Here's how it works:

    When you upload data to PEP:

    1. Your device retrieves the public keys of two different PEP servers.
    2. Your device encrypts the data in two layers using both public keys.
    3. The **Storage Facility** (one of the PEP servers) stores data with two layers of encryption and cannot read it.

    When you download data from PEP:

    1. A PEP server re-encrypts the data with your public key.
    2. Access is checked, and one layer of encryption is removed by one server.
    3. Access is logged by a different PEP server, and another layer of encryption is removed.
    4. After both servers process the data, the remaining encryption is your public key.
    5. You receive the data and decrypt it locally using your private key.

    The ability to remove one layer of encryption while the data remains encrypted with other keys is called **polymorphic encryption**.

    This cryptographic design means that privacy protection is built into the fundamental architecture, not just added as a feature. The system is designed so that no single party can decrypt the data alone, data remains confidential *even if servers are compromised* and *even the server administrators cannot read your data or link records across User Groups*.

### Creating the User Group

Now that the **Subject Group** and **Column Group** exist, let's make the **User Group** and its access privileges in order. To do this, we need to log in as the **Access Administrator**. Again, in a real life scenario, this would be a different person from the **Data Administrator**.

- Use the `pepLogon` command again to log in again and switch **User Group** to **Access Administrator**.

- Create a **User Groups** for analysts:

```shell
pepcli user group create Analyst
```

- Give `Analyst` read access to the `Grades` **Column Group** by creating **c**olumn **g**roup **a**ccess **r**ules (cgar):

```shell
pepcli ama cgar create Grades Analyst read
```

- Give `Analyst` write access to the `Average` **Column Group**, so that they can upload their findings back to the repository. We also want the `Research Assessor` to be able to read the column so that they can report the grade average back to the student:

```shell
pepcli ama cgar create Average Analyst write
pepcli ama cgar create Average "Research Assessor" read
```

- Give `Analyst` access to the second year students by creating **p**articipant **g**roup **a**ccess **r**ules (pgar):

```shell
pepcli ama pgar create SecondYearStudents Analyst access
pepcli ama pgar create SecondYearStudents Analyst enumerate
```

The `access` and `enumerate` keywords are access modes needed for downloading data. Their technical details are outside this tutorial's scope.

### Checking progress

Everything should be in order now. Let's verify by querying the repository for the current administration status. Display an overview of the **Column Groups**, **Subject Groups**, and **Access Rules** by querying the **Access Manager** administration (ama):

```shell
pepcli ama query
```

The output comprises a few sections:

1. **Columns**
2. **Column Groups**
3. **Column Group Access Rules**
4. **(Subject/Participant) Groups**
5. **(Subject/Participant) Group Access Rules**

Look in all sections and see whether or not your newly created administration is present.

- Display an overview of the **User Groups**. This output is much less elaborate:

```shell
pepcli user query
```

- Check whether your **User Groups** exist.

### Download as Analyst

- Use the `pepLogon` command and switch to the newly created `Analyst` **User Group**.
- Download the accessible data again and export to `.csv`:

```shell
pepcli pull --all-accessible --export csv --force
```

```shell
cat export.csv
```

You should now only see the grades for second-year students. Notice that the pseudonyms are different again, you're now using the **Local Pseudonyms** for the `Analyst` **User Group**.

1. Calculate an average for one of the students. One student has an 8 in French, and a 3 in Maths. The average of these two grades is 5.5

   Upload this average using the pseudonym:

    ```shell
    pepcli store --participant <PSEUDONYM> --column Average --data "5.5"
    ```

    Replace `<PSEUDONYM>` with the pseudonym corresponding to these grades from your pulled data.

2. (Optional) Repeat for other students, calculate their averages and upload them.

3. Verify that you made your uploaded data available to the **Research Assessor**:

    ```shell
    pepLogon
    ```

    Select **Research Assessor** from the menu, and then:

    ```shell
    pepcli pull --all-accessible --export csv --force
    cat export.csv
    ```

    You should now see the Average column populated with your computed values! Apparently the student with a 5.5 average was `Alice`.

## Exploring further

Congratulations on completing the tutorial! You now have a working understanding of PEP's core concepts. The tutorial environment is still running, so feel free to explore further and experiment with the system.

### Getting help

Every `pepcli` command supports the `-h` or `--help` flag to display detailed usage information. For example:

```shell
pepcli -h        # Show all available commands
pepcli store -h  # Show options for the store command
pepcli ama -h    # Show Access Manager administration commands
pepcli user -h   # Show user management commands
```

you can also use the ++tab++ key to auto-complete commands and parameters in the terminal, which can help you discover available options.

### Try it yourself

Here are some ideas to explore:

- Add more columns and organize them into **Column Groups**.
- Register more subjects and build larger **Subject Groups**.
- Create additional **User Groups** with different access permissions.
- Use `pepcli delete` to delete data.
- Use `pepcli export` for different export formats (e.g., `.json`, `.yaml`).

The environment is safe to experiment in. You can't break anything, and you can always restart the container to begin fresh. When you're done exploring, remember to clean up:

```shell
exit  # Leave the container
docker stop pep-sandbox # Stop the container
```

## Wrapping up

In this tutorial we have explored the basics of PEP. We registered a new subject and stored data for them across multiple columns. We then demonstrated PEP's selective data access by creating an `Analyst` **User Group**, with access to a different subset of the data. By creating appropriate **Column Groups**, **Subject Groups**, and access rules according to the *four-eyes principle*, we showed how an analyst can access only the data relevant to their specific research question, demonstrating the principles of *data minimisation* and **polymorphic pseudonymisation**. Furthermore, we explored the concept of **polymorphic encryption** and how it distributes trust across multiple servers to enhance data security.

Thank you for your participation in this tutorial! If you have any questions or comments, please let us know at <support@pep.cs.ru.nl>.

## Bonus: Using Reference IDs for privacy-preserving data upload

!!! note "Pre-configured"
    To avoid repetition, a History TA **User Group** with write access to the history grades was preconfigured in the tutorial environment.

In this bonus section, we'll explore **Reference IDs**, a feature that allows external parties to upload data using pseudonyms, without being able to access or link data across the repository.

Imagine this workflow:

- A **Research Assessor** registers students and generates **Reference IDs** for each student.
- These **Reference IDs** are distributed to teachers (e.g., printed on paper slips given to students).
- A History TA receives completed history assignments with only the **Reference ID** on them.
- The History TA uploads grades using these **Reference IDs** without seeing any other student data.
- The TA cannot see names, addresses, or grades from other subjects.

**Reference IDs** are static identifiers used for a single purpose, such as linking a physical specimen, a file upload, a survey response etc. to PEP. They are simple, human-readable identifiers (like `RIDHI8593615`, `RIDHI7697529`) stored in dedicated columns.

In our example, the History TA uses **Reference IDs** to upload grades to a second year history exam, without ever seeing student names or other grades.

1. First we need to set the History TA up to access the previously created SecondYearStudents **Subject Group**. Log in as **Access Administrator**,

    ```shell
    pepLogon
    ```

  Create the History TA **User Group**:

    ```shell
    pepcli user group create "History TA"
    ```

  and create the necessary access rules:

    ```shell
    pepcli ama cgar create History_Grade "History TA" write
    pepcli ama cgar create ShortPseudonym.History "History TA" read
    pepcli ama pgar create SecondYearStudents "History TA" access
    pepcli ama pgar create SecondYearStudents "History TA" enumerate
    pepcli ama pgar create SecondYearStudents "Research Assessor" access
    pepcli ama pgar create SecondYearStudents "Research Assessor" enumerate
    ```

2. Let's now obtain the **Reference IDs** for our students:

    ```shell
    pepLogon
    ```

  Select **Research Assessor**, then, first generate new **Reference IDs** for the newly registered students:

    ```shell
    pepcli register ensure-complete
    ```

  Then list the **Reference IDs** for the second year students:

    ```shell
    pepcli list -C ShortPseudonyms -c Name -P SecondYearStudents
    ```

  You'll see **Reference IDs** (named `ShortPseudonym.History`) like `RIDHI4019677` paired with student names. In a real scenario, these could be made into QR codes, distributed via email, or printed on paper slips for students to use when submitting assignments. Distribute these **Reference IDs** to the History TA (i.e. copy them, write them down or just scroll up in the terminal later to copy them).

3. Now log in as `History TA`:

    ```shell
    pepLogon
    ```

    Select `History TA` from the menu.

4. As `History TA`, you don't have any read access (you can check this), but you can upload to the `History_Grade` column using the **Reference IDs**. Upload a history grade using one of the **Reference IDs** you listed earlier:

    ```shell
    pepcli store --sp <REFERENCE_ID> --column History_Grade --data "7.5"
    ```

    Replace `<REFERENCE_ID>` with one of the identifiers from the list (e.g., `RIDHI4019677`).

5. (Optional) Upload more history grades for other students:

6. Verify your uploaded data:

    ```shell
    pepLogon
    ```

    Select `Analyst` from the menu, then:

    ```shell
    pepcli pull --all-accessible --export csv --force
    cat export.csv
    ```

You should see the `History_Grade` column populated with the grades the `History TA` uploaded, without anyone that does not already have primary contact with the students (including servers) requiring any personal information. This demonstrates how PEP enables privacy-preserving data uploads.

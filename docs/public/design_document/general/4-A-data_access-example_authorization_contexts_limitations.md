# Data Access - Example: Limitations within Authorization Contexts

All cells that are part of the **Column Groups** and **Data Subject Groups** of an **Authorization Context** are included in the **Authorization Context**. If the data subset spanned by Row 2 and Column 3 should be combined with the data subset spanned by Row 5 and Column 7, the subsets spanned by Row 2 and Column 7, and the subset spanned by Row 5 and Column 3 are automatically also part of the **Authorization Context**.

## Example

Subset **A** is the combination of a **Column Group** containing C1 and C2 and a **Data Subject Group** containing P2 and P4. This gives access to 4 cells:

<style>
.example-table-A td:nth-child(2),
.example-table-A td:nth-child(3),
.example-table-A tr:nth-child(2),
.example-table-A tr:nth-child(4)
{
  background: grey;
  border-color: white;
  font-weight: bold;
}
</style>
<div class="example-table-A">

|        | C1 | C2 | C3 | C4 |
|--------|----|----|----|----|
| **P1** |    |    |    |    |
| **P2** | 1  | 1  |    |    |
| **P3** |    |    |    |    |
| **P4** | 1  | 1  |    |    |

</div>

Subset **B** is the combination of a **Column Group** containing C2 and C3 and a **Data Subject Group** containing P2 and P3.
This also gives access to 4 cells:

<style>
.example-table-B td:nth-child(2),
.example-table-B td:nth-child(4),
.example-table-B tr:nth-child(2),
.example-table-B tr:nth-child(3)
{
  background: grey;
  border-color: white;
  font-weight: bold;
}
</style>
<div class="example-table-B">

|        | C1 | C2 | C3 | C4 |
|--------|----|----|----|----|
| **P1** |    |    |    |    |
| **P2** | 1  |    | 1  |    |
| **P3** | 1  |    | 1  |    |
| **P4** |    |    |    |    |

</div>

A **User** with access to both authorization groups, **A** and **B**, can access all cells where both the **Column** and **Data Subject** are accessible via either **A** *or* **B**.
This results in access to 9 **Cells** in total, two of which were not accessible via **A** nor **B** (marked `!`):

<style>
.example-table-AB td:nth-child(2),
.example-table-AB td:nth-child(3),
.example-table-AB td:nth-child(4),
.example-table-AB tr:nth-child(2),
.example-table-AB tr:nth-child(3),
.example-table-AB tr:nth-child(4)
{
  background: grey;
  border-color: white;
  font-weight: bold;
}
</style>
<div class="example-table-AB">

|        | C1 | C2 | C3 | C4 |
|--------|----|----|----|----|
| **P1** |    |    |    |    |
| **P2** | 1  | 1  | 1  |    |
| **P3** | 1  | 1! | 1  |    |
| **P4** | 1   | 1  | 1! |    |

</div>

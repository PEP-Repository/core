# PEP Connector

PEP Connector is a Python package that provides a simple way to interact with both a PEP Repository and various APIs using a single package. It is a wrapper around the PEP Command Line Interface that allows you to easily make requests to a PEP Repository, an external data source (e.g., Google Forms or LimeSurvey API), and interchange the results in a Pythonic way.

## Installation

For now the connector is not available on pip, so you need to install it from the source code using the following command:

```bash
pip install pep_connector-{version}(-{build tag})?-{python tag}-{abi tag}-{platform tag}.whl
```

## Usage

For example, if you want to create a few new columns in a PEP Repository, you can use the following code:

```python
from pep_connector import PEPRepository
from pep_connector import DataAdmin

repository = PEPRepository(config="config.yml")

repository.authenticate("Data Administrator")

data_admin = DataAdmin(repository)

columns = ["column1", "column2", "column3"]

data_admin.create_columns(columns)
```

# Introduction

In 2016 the Personalized Parkinson Project **(&bullet; 1)** was started as a joint research project between Radboudumc and Verily inc., an Alphabet **(&bullet; 2)** company. 

The research is focused on investigating causes of the disease, and progression of the disease over time, with special emphasis on personalizing care and support for patients suffering from Parkinsons disease. Because of the nature of the project, where a large body of sensitive personal data is created for scientific research by many different parties, special attention was necessary to protect the research data from unauthorized use and for compliance with the GDPR.  Implementing security and privacy by design requires a combination of legal, organizational and technical measures **(&bullet; 3)**. To achieve part of this objective, the PEP project was started to provide tooling for secure and privacy preserving production and sharing of data for research purposes.

PEP stands for Polymorphic Encryption and Pseudonymisation, a cryptographic technique that forms the base of a newly developed repository for the sharing of scientific data. The Digital Security group of Radboud University started the PEP-project for the design, development, and implementation of such a repository in 2016. In March 2017, a global description of all use cases has been drafted and agreed upon with the PPP.

The first use case was that of the Research Assessor.  The assessor client was designed and implemented, and derived from that, the monitor client (for an external auditor), in order to support the first phase of the Personalized Parkinson’s Project (PPP).  Following that, parts of the core PEP system were technically redesigned and/or refactored based on gained insights during the first year of development. This resulted in increased stability and maintainability of the system. In addition, the system has been deployed to the designated production infrastructure, and is currently in use by several research projects.

In 2019 the PEP-project was incorporated in iHub, Radboud's interdisciplinary research hub on digitalization and society.

The *PEP design document* describes the functional design of the PEP-based research data repository. Many of the **Bold Faced Terms** used in this document can be found in the [Glossary](../../user_documentation/glossary.md) of the User Documentation.

> **(&bullet;)** Notes:
>
> * [1] The Personalized Parkinson Project (PPP) also goes by the Dutch name Parkinson op Maat (POM): <https://www.parkinsonopmaat.nl/onderzoeken>
>
> * [2] Alphabet For an elaborative description of the PPP, see the scientific resource paper: <https://bmcneurol.biomedcentral.com/articles/10.1186/s12883-019-1394-3>
>
> * [3] Combination of legal, organizational and technical measures: Medical research, Big Data and the need for privacy by design: <https://journals.sagepub.com/doi/10.1177/2053951718824352>

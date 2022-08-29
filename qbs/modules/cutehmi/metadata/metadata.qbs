import qbs
import qbs.File
import qbs.FileInfo
import qbs.Process
import qbs.TextFile
import qbs.Utilities

/**
  This module collects properties defined within Qbs product and generates 'cutehmi.metadata.json' and 'cutehmi.metadata.hpp'
  artifacts.
  */
Module {
	additionalProductTypes: ["cutehmi.metadata"]

	PropertyOptions {
		name: "artifacts"
		description: "Whether to generate any artifacts."
	}
	property bool artifacts: true

	property path metadataHppArtifact: artifacts && product.cpp !== undefined ? "cutehmi.metadata.hpp" : undefined

	property path metadataJsonArtifact: artifacts ? product.name + ".metadata.json" : undefined

	Depends { name: "Qt.core" }
	Qt.core.resourcePrefix: product.name

	Rule {
		condition: metadataHppArtifact !== undefined
		multiplex: true

		prepare: {
			var productRequiredProperties = [
						'name',
						'cutehmiType',
						'major',
						'minor',
						'micro',
						'friendlyName',
						'vendor',
						'domain',
						'description',
						'i18n',
			]
			for (var prop in productRequiredProperties)
				if (product[productRequiredProperties[prop]] === undefined)
					console.warn("Undefined property 'product." + productRequiredProperties[prop] +"' in '" + product.name + "' (product directory: '" + product.sourceDirectory + "').")

			var cmd = new JavaScriptCommand();
			cmd.description = "generating " + output.filePath
			cmd.highlight = "codegen";
			cmd.sourceCode = function() {
				var metadata = {
					"_comment": "This file has been autogenerated by 'cutehmi.metadata' Qbs module.",
					"name": product.name,
					"cutehmiType": product.cutehmiType,
					"major": product.major,
					"minor": product.minor,
					"micro": product.micro,
					"friendlyName": product.friendlyName,
					"vendor": product.vendor,
					"domain": product.domain,
					"description": product.description,
					"i18n": product.i18n,
					"dependencies": []
				}

				for (i in product.dependencies) {
					var dependency = product.dependencies[i]
					var metadataDepencency = {
						"name": dependency.name,
					}
					metadata.dependencies.push(metadataDepencency)
				}

				var f = new TextFile(output.filePath, TextFile.WriteOnly);
				try {
					f.write(JSON.stringify(metadata))
				} finally {
					f.close()
				}
			}

			return [cmd]
		}

		Artifact {
			filePath: product.cutehmi.metadata.metadataJsonArtifact
			fileTags: ["cutehmi.metadata", "cutehmi.metadata.json"]
		}
	}

	Rule {
		condition: metadataJsonArtifact !== undefined
		multiplex: true

		prepare: {
			var cmd = new JavaScriptCommand();
			cmd.description = "generating " + product.sourceDirectory + "/cutehmi.metadata.hpp"
			cmd.highlight = "codegen";
			cmd.sourceCode = function() {
				var f = new TextFile(product.sourceDirectory + "/cutehmi.metadata.hpp", TextFile.WriteOnly);
				try {
					var shortPrefix = product.baseName.toUpperCase().replace(/\./g, '_')
					var prefix = product.name.toUpperCase().replace(/\./g, '_')

					f.writeLine("#ifndef " + prefix + "_METADATA_HPP")
					f.writeLine("#define " + prefix + "_METADATA_HPP")

					f.writeLine("")
					f.writeLine("#include <QCoreApplication>")

					f.writeLine("")
					f.writeLine("// This file has been autogenerated by 'cutehmi.metadata' Qbs module.")
					f.writeLine("")

					f.writeLine("#define " + shortPrefix + "_NAME \"" + product.name + "\"")
					f.writeLine("#define " + shortPrefix + "_CUTEHMI_TYPE \"" + product.cutehmiType + "\"")
					f.writeLine("#define " + shortPrefix + "_FRIENDLY_NAME \"" + product.friendlyName + "\"")
					f.writeLine("#define " + shortPrefix + "_VENDOR \"" + product.vendor + "\"")
					f.writeLine("#define " + shortPrefix + "_DOMAIN \"" + product.domain + "\"")
					f.writeLine("#define " + shortPrefix + "_VERSION \"" + product.major + "." + product.minor + "." + product.micro + "\"")
					f.writeLine("#define " + shortPrefix + "_MAJOR " + product.major + "")
					f.writeLine("#define " + shortPrefix + "_MINOR " + product.minor + "")
					f.writeLine("#define " + shortPrefix + "_MICRO " + product.micro + "")
					f.writeLine("#define " + shortPrefix + "_TRANSLATED_FRIENDLY_NAME QCoreApplication::translate(\"metadata\", \"" + product.friendlyName + "\")")
					f.writeLine("#define " + shortPrefix + "_TRANSLATED_DESCRIPTION QCoreApplication::translate(\"metadata\", \"" + product.description + "\")")
					f.writeLine("#define " + shortPrefix + "_I18N " + (product.i18n ? "true" : "false") + "")

					f.writeLine("")
					f.writeLine("#ifdef " + shortPrefix + "_BUILD")
					f.writeLine("  #define " + prefix + "_" + product.minor)
					f.writeLine("#endif")

					for (var m = product.minor; m > 0; m--) {
						f.writeLine("")
						f.writeLine("#ifdef " + prefix + "_" + m)
						f.writeLine("  #ifndef " + prefix + "_CURRENT ")
						f.writeLine("    #define " + prefix + "_CURRENT " + m)
						f.writeLine("  #endif")
						f.writeLine("  #define " + prefix + "_" + (m - 1))
						f.writeLine("#endif")
					}

					f.writeLine("")
					f.writeLine("#ifndef " + prefix + "_CURRENT ")
					f.writeLine("  #define " + prefix + "_CURRENT 0")
					f.writeLine("#endif")

					f.writeLine("")
					f.writeLine("#endif")
				} finally {
					f.close()
				}
			}

			return [cmd]
		}

		Artifact {
			filePath: product.cutehmi.metadata.metadataHppArtifact
			fileTags: ["cutehmi.metadata", "cutehmi.metadata.hpp", "hpp"]
		}
	}
}

//(c)C: Copyright © 2018-2020, Michał Policht <michal@policht.pl>. All rights reserved.
//(c)C: SPDX-License-Identifier: LGPL-3.0-or-later OR MIT
//(c)C: This file is a part of CuteHMI.
//(c)C: CuteHMI is free software: you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
//(c)C: CuteHMI is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more details.
//(c)C: You should have received a copy of the GNU Lesser General Public License along with CuteHMI.  If not, see <https://www.gnu.org/licenses/>.
//(c)C: Additionally, this file is licensed under terms of MIT license as expressed below.
//(c)C: Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
//(c)C: The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
//(c)C: THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
